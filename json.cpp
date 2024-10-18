#include "json.h"

using namespace std;

namespace json {
    namespace {
        using Number = std::variant<int, double>;

        Number LoadNumber(std::istream& input) {
            using namespace std::literals;
            std::string parsed_num;
            auto read_char = [&parsed_num, &input] {
                parsed_num += static_cast<char>(input.get());
                if (!input) {
                    throw ParsingError("Failed to read number from stream"s);
                }
            };
            // Считывает одну или более цифр в parsed_num из input
            auto read_digits = [&input, read_char] {
                if (!std::isdigit(input.peek())) {
                    throw ParsingError("A digit is expected"s);
                }
                while (std::isdigit(input.peek())) {
                    read_char();
                }
            };
            if (input.peek() == '-') {
                read_char();
            }
            // Парсим целую часть числа
            if (input.peek() == '0') {
                read_char();
                // После 0 в JSON не могут идти другие цифры
            }
            else {
                read_digits();
            }
            bool is_int = true;
            // Парсим дробную часть числа
            if (input.peek() == '.') {
                read_char();
                read_digits();
                is_int = false;
            }
            // Парсим экспоненциальную часть числа
            if (int ch = input.peek(); ch == 'e' || ch == 'E') {
                read_char();
                if (ch = input.peek(); ch == '+' || ch == '-') {
                    read_char();
                }
                read_digits();
                is_int = false;
            }
            try {
                if (is_int) {
                    try {
                        return std::stoi(parsed_num);
                    }
                    catch (...) {
                        // В случае неудачи, например, при переполнении,
                        // код ниже попробует преобразовать строку в double
                    }
                }
                return std::stod(parsed_num);
            }
            catch (...) {
                throw ParsingError("Failed to convert "s + parsed_num + " to number"s);
            }
        }
        // Считывает содержимое строкового литерала JSON-документа
        // Функцию следует использовать после считывания открывающего символа ":
        std::string LoadString(std::istream& input) {
            using namespace std::literals;
            auto it = std::istreambuf_iterator<char>(input);
            auto end = std::istreambuf_iterator<char>();
            std::string s;
            while (true) {
                if (it == end) {
                    // Поток закончился до того, как встретили закрывающую кавычку?
                    throw ParsingError("String parsing error");
                }
                const char ch = *it;
                if (ch == '"') {
                    // Встретили закрывающую кавычку
                    ++it;
                    break;
                }
                else if (ch == '\\') {
                    // Встретили начало escape-последовательности
                    ++it;
                    if (it == end) {
                        // Поток завершился сразу после символа обратной косой черты
                        throw ParsingError("String parsing error");
                    }
                    const char escaped_char = *(it);
                    // Обрабатываем одну из последовательностей: \\, \n, \t, \r, \"
                    switch (escaped_char) {
                        case 'n':
                            s.push_back('\n');
                            break;
                        case 't':
                            s.push_back('\t');
                            break;
                        case 'r':
                            s.push_back('\r');
                            break;
                        case '"':
                            s.push_back('"');
                            break;
                        case '\\':
                            s.push_back('\\');
                            break;
                        default:
                            // Встретили неизвестную escape-последовательность
                            throw ParsingError("Unrecognized escape sequence \\"s + escaped_char);
                    }
                }
                else if (ch == '\n' || ch == '\r') {
                    // Строковый литерал внутри- JSON не может прерываться символами \r или \n
                    throw ParsingError("Unexpected end of line"s);
                }
                else {
                    // Просто считываем очередной символ и помещаем его в результирующую строку
                    s.push_back(ch);
                }
                ++it;
            }
            return s;
        }

        Node LoadNode(istream& input);

        Node LoadArray(istream& input) {
            Array result;
            for (char c; input >> c && c != ']';) {
                if (c != ',') {
                    input.putback(c);
                }
                result.push_back(LoadNode(input));
            }
            return Node(move(result));
        }

        Node LoadNum(istream& input) {
            auto result =LoadNumber(input);
            if (holds_alternative<int>(result)) {
                return Node{ get<int>(result) };
            }
            else {
                return Node{ get<double>(result) };
            }
        }

        Node LoadStr(istream& input) {
            string line = LoadString(input);
            return Node(move(line));
        }

        Node LoadDict(istream& input) {
            Dict result;
            for (char c; input >> c && c != '}';) {
                if (c == ',') {
                    input >> c;
                }
                string key = LoadStr(input).AsString();
                input >> c;
                result.insert({ move(key), LoadNode(input) });
            }
            return Node(move(result));
        }

        Node LoadNull() {
            return Node{ nullptr };
        }

        Node LoadBool(istream& input) {
            char c;
            input >> c;
            if (c == 't') {
                return Node{ true };
            }
            else {
                return Node{ false };
            }
        }

        Node LoadNode(istream& input) {
            char c;
            input >> c;
            if (c == '[') {
                return LoadArray(input);
            }
            else if (c == '{') {
                return LoadDict(input);
            }
            else if (c == '"') {
                return LoadStr(input);
            }
            else if (c == 'n') {
                return LoadNull();
            }
            else if (c == 't' || c == 'f') {
                input.putback(c);
                return LoadBool(input);
            }
            else {
                input.putback(c);
                return LoadNum(input);
            }
        }
    }

    Node::Node(Value value) : value_(std::move(value)) {}
    Node::Node(Array array) : value_(std::move(array)) {}
    Node::Node(Dict map) : value_(std::move(map)) {}

    Node::Node(std::nullptr_t) : value_(nullptr) {}
    Node::Node(int value) : value_(value) {}
    Node::Node(double value) : value_(value) {}
    Node::Node(std::string value) : value_(std::move(value)) {}
    Node::Node(const char* value) : value_(std::string(value)) {}
    Node::Node(bool value) : value_(value) {}


    Node::Node(std::initializer_list<Node> list) : value_(Array(list.begin(), list.end())) {}
    Node::Node(std::initializer_list<std::pair<std::string, Node>> list)
            : value_(Dict(list.begin(), list.end())) {}

    const Node::Value& Node::GetValue() const {
        return value_;
    }

    bool Node::IsInt() const {
        return std::holds_alternative<int>(value_);
    }

    bool Node::IsDouble() const {
        return std::holds_alternative<int>(value_) || std::holds_alternative<double>(value_);
    }

    bool Node::IsPureDouble() const {
        return std::holds_alternative<double>(value_);
    }

    bool Node::IsBool() const {
        return std::holds_alternative<bool>(value_);
    }

    bool Node::IsString() const {
        return std::holds_alternative<std::string>(value_);
    }

    bool Node::IsNull() const {
        return std::holds_alternative<std::nullptr_t>(value_);
    }

    bool Node::IsArray() const {
        return std::holds_alternative<Array>(value_);
    }

    bool Node::IsMap() const {
        return std::holds_alternative<Dict>(value_);
    }

    int Node::AsInt() const {
        if (std::holds_alternative<int>(value_)) {
            return std::get<int>(value_);
        }
        else {
            throw std::logic_error("incorrect data type");
        }
    }

    bool Node::AsBool() const {
        if (std::holds_alternative<bool>(value_)) {
            return std::get<bool>(value_);
        }

        else {
            throw std::logic_error("incorrect data type");
        }
    }

    double Node::AsDouble() const {//Возвращает значение типа double, если внутри хранится double либо int. В последнем случае возвращается приведённое в double значение
        if (std::holds_alternative<double>(value_)) {
            return std::get<double>(value_);
        }
        else if (std::holds_alternative<int>(value_)) {
            return static_cast<double>(std::get<int>(value_));
        }

        else {
            throw std::logic_error("incorrect data type");
        }
    }

    const std::string& Node::AsString() const {
        if (std::holds_alternative<std::string>(value_)) {
            return std::get<std::string>(value_);
        }
        else {
            throw std::logic_error("incorrect data type");
        }

    }

    const Array& Node::AsArray() const {
        if (std::holds_alternative<Array>(value_)) {
            return std::get<Array>(value_);
        }
        else {
            throw std::logic_error("incorrect data type");
        }
    }

    const Dict& Node::AsMap() const {
        if (std::holds_alternative<Dict>(value_)) {
            return std::get<Dict>(value_);
        }
        else {
            throw std::logic_error("incorrect data type");
        }
    }



    bool operator== (const Node& left, const Node& right) {
        return left.GetValue() == right.GetValue();
    }

    bool operator!= (const Node& left, const Node& right) {
        return left.GetValue() != right.GetValue();
    }



    Document::Document(Node root)
            : root_(move(root)) {
    }



    const Node& Document::GetRoot() const {
        return root_;
    }



    Document Load(istream& input) {
        return Document{ LoadNode(input) };
    }



    std::string PrintString(std::string value) {
        std::string s;
        s += "\"";
        for (size_t i = 0; i < value.size(); ++i) {
            switch (value[i]) {
                case '"':
                    s += "\\\"";
                    break;
                case '\r':
                    s += "\\r";
                    break;
                case '\n':
                    s += "\\n";
                    break;
                case '\t':
                    s += "\\t";
                    break;
                case '\\':
                    s += "\\\\";
                    break;
                default:
                    s.push_back(value[i]);
            }
        }
        s += "\"";
        return s;
    }


    struct NodePrinter {
        std::ostream& out;
        void operator() (std::nullptr_t) const {
            out << "null";
        }

        void operator() (int value) const {
            out << value;
        }

        void operator() (double value) const {
            out << value;
        }

        void operator() (std::string value) const {
            out << PrintString(value);
        }

        void operator() (bool value) const {
            out << std::boolalpha << value;
        }

        void operator() (Array value) const {
            out << value;
        }

        void operator() (Dict value) const {
            out << value;
        }
    };



    std::ostream& operator<< (std::ostream& out, const Array& values) {
        out << "[";
        for (size_t i = 0; i < values.size() - 1; ++i) {
            std::visit(NodePrinter{ out }, values[i].GetValue());
            out << ", ";
        }

        std::visit(NodePrinter{ out }, values[values.size() - 1].GetValue());
        out << "]";
        return out;
    }



    std::ostream& operator<< (std::ostream& out, const Dict& values) {
        out << "{";
        size_t count = 0;
        for (const auto& val : values) {
            if (count != values.size() - 1) {
                out << "\"" << val.first << "\": ";
                std::visit(NodePrinter{ out }, val.second.GetValue());
                out << ", ";
            }

            else {
                out << "\"" << val.first << "\": ";
                std::visit(NodePrinter{ out }, val.second.GetValue());
            }
        }
        out << "}";
        return out;
    }



    void Print(const Document& doc, std::ostream& output) {
        std::visit(NodePrinter{ output }, doc.GetRoot().GetValue());
    }
}