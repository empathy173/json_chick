#pragma once

#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace json {
    class Node;
    // Сохраните объявления Dict и Array без изменения
    using Dict = std::map<std::string, Node>;

    using Array = std::vector<Node>;
    // Эта ошибка должна выбрасываться при ошибках парсинга JSON
    class ParsingError : public std::runtime_error {
    public:
        using runtime_error::runtime_error;
    };

    class Node {
    public:
        /* Реализуйте Node, используя std::variant */
        using Value = std::variant<std::nullptr_t, int, double, std::string, bool, Array, Dict>;
        const Value& GetValue() const;
        Node(Value value);
        //Node(Array value);
        Node() = default;
        Node(Array array);
        Node(Dict map);
        Node(std::nullptr_t);
        Node(int value);
        Node(double value);
        Node(std::string value);
        Node(const char* value);
        Node(bool value);
        Node(std::initializer_list<Node> list);
        Node(std::initializer_list<std::pair<std::string, Node>> list);

        //Следующие методы Node сообщают, хранится ли внутри значение некоторого типа :
        bool IsInt() const;
        bool IsDouble() const;//Возвращает true, если в Node хранится int либо double
        bool IsPureDouble() const;//Возвращает true, если в Node хранится double
        bool IsBool() const;
        bool IsString() const;
        bool IsNull() const;
        bool IsArray() const;
        bool IsMap() const;
        //Ниже перечислены методы, которые возвращают хранящееся внутри Node значение заданного типа.
        //Если внутри содержится значение другого типа, должно выбрасываться исключение std::logic_error
        int AsInt() const;
        bool AsBool() const;
        double AsDouble() const;//Возвращает значение типа double, если внутри хранится double либо int. В последнем случае возвращается приведённое в double значение
        const std::string& AsString() const;
        const Array& AsArray() const;
        const Dict& AsMap() const;
    private:
        Value value_;
    };
    bool operator== (const Node& left, const Node& right);
    bool operator!= (const Node& left, const Node& right);

    class Document {
    public:
        Document(Node root);
        const Node& GetRoot() const;

    private:
        Node root_;
    };

    Document Load(std::istream& input);

    std::ostream& operator<< (std::ostream& out, const Array& values);
    std::ostream& operator<< (std::ostream& out, const Dict& values);

    void Print(const Document& doc, std::ostream& output);
}  // namespace json