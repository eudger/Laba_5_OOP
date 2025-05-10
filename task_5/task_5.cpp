// task_5.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <stack>
#include <memory>
#include <algorithm>
#include <sstream>

// Base class Element
class Element {
public:
    virtual ~Element() = default;
    virtual void print(int indent = 0) const = 0;
    virtual std::unique_ptr<Element> clone() const = 0;
    virtual void move(float dx, float dy) = 0;
    virtual Element* find(float x, float y) = 0;
    float x, y;
};

// Circle class
class Circle : public Element {
public:
    Circle(float x_, float y_, float r_) : r(r_) { x = x_; y = y_; }
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Circle at (" << x << ", " << y << ") with radius " << r << std::endl;
    }
    std::unique_ptr<Element> clone() const override { return std::make_unique<Circle>(*this); }
    void move(float dx, float dy) override { x += dx; y += dy; }
    Element* find(float x_, float y_) override {
        float dx = x_ - x;
        float dy = y_ - y;
        return (dx * dx + dy * dy <= r * r) ? this : nullptr;
    }
private:
    float r;
};

// Rectangle class
class Rectangle : public Element {
public:
    Rectangle(float x_, float y_, float w_, float h_) : w(w_), h(h_) { x = x_; y = y_; }
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Rectangle at (" << x << ", " << y << ") with width " << w << " and height " << h << std::endl;
    }
    std::unique_ptr<Element> clone() const override { return std::make_unique<Rectangle>(*this); }
    void move(float dx, float dy) override { x += dx; y += dy; }
    Element* find(float x_, float y_) override {
        return (x_ >= x && x_ <= x + w && y_ >= y && y_ <= y + h) ? this : nullptr;
    }
private:
    float w, h;
};

// Group class
class Group : public Element {
public:
    Group(float x_, float y_) { x = x_; y = y_; }
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Group at (" << x << ", " << y << ") with elements:" << std::endl;
        for (const auto& e : elements_) e->print(indent + 2);
    }
    std::unique_ptr<Element> clone() const override {
        auto g = std::make_unique<Group>(x, y);
        for (const auto& e : elements_) g->add(e->clone());
        return g;
    }
    void move(float dx, float dy) override {
        x += dx; y += dy;
        for (auto& e : elements_) e->move(dx, dy);
    }
    Element* find(float x_, float y_) override {
        if (std::abs(x - x_) < 1e-5 && std::abs(y - y_) < 1e-5) return this;
        for (auto& e : elements_) {
            if (Element* found = e->find(x_ - x, y_ - y)) return found;
        }
        return nullptr;
    }
    void add(std::unique_ptr<Element> e) { elements_.push_back(std::move(e)); }
    void remove(Element* e) {
        elements_.erase(std::remove_if(elements_.begin(), elements_.end(),
            [e](const auto& ptr) { return ptr.get() == e; }), elements_.end());
    }
    const std::vector<std::unique_ptr<Element>>& getElements() const { return elements_; }
private:
    std::vector<std::unique_ptr<Element>> elements_;
};

// Abstract command for undo/redo
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

// Command for adding an element
class AddCommand : public Command {
public:
    AddCommand(Group* target, std::unique_ptr<Element> element)
        : target_(target), element_(std::move(element)) {}
    void execute() override { target_->add(std::move(element_)); }
    void undo() override { target_->remove(element_.get()); }
private:
    Group* target_;
    std::unique_ptr<Element> element_;
};

// Command for removing an element
class RemoveCommand : public Command {
public:
    RemoveCommand(Group* target, Element* element)
        : target_(target), element_(element), clone_(element->clone()) {}
    void execute() override { target_->remove(element_); }
    void undo() override { target_->add(clone_->clone()); }
private:
    Group* target_;
    Element* element_;
    std::unique_ptr<Element> clone_;
};

// Command for moving an element
class MoveCommand : public Command {
public:
    MoveCommand(Element* element, float dx, float dy)
        : element_(element), dx_(dx), dy_(dy) {}
    void execute() override { element_->move(dx_, dy_); }
    void undo() override { element_->move(-dx_, -dy_); }
private:
    Element* element_;
    float dx_, dy_;
};

// Command for moving an element to another group
class MoveToGroupCommand : public Command {
public:
    MoveToGroupCommand(Group* source, Group* target, Element* element)
        : source_(source), target_(target), element_(element), clone_(element->clone()) {}
    void execute() override {
        target_->add(clone_->clone());
        source_->remove(element_);
    }
    void undo() override {
        target_->remove(clone_.get());
        source_->add(clone_->clone());
    }
private:
    Group* source_;
    Group* target_;
    Element* element_;
    std::unique_ptr<Element> clone_;
};

// Iterator for traversing elements
class ElementIterator {
public:
    ElementIterator(const Group* root) { pushGroup(root); }
    bool hasNext() const { return !stack_.empty(); }
    const Element* next() {
        if (stack_.empty()) return nullptr;
        while (!stack_.empty()) {
            auto& current = stack_.back();
            if (current.index < current.group->getElements().size()) {
                const Element* e = current.group->getElements()[current.index++].get();
                if (auto* g = dynamic_cast<const Group*>(e)) {
                    pushGroup(g);
                }
                return e;
            }
            stack_.pop_back();
        }
        return nullptr;
    }
private:
    struct StackFrame {
        const Group* group;
        size_t index;
        StackFrame(const Group* g) : group(g), index(0) {}
    };
    std::vector<StackFrame> stack_;
    void pushGroup(const Group* g) { stack_.emplace_back(g); }
};

// Editor class (Facade)
class Editor {
public:
    Editor() : root_(std::make_unique<Group>(0, 0)) {
        groups_.push_back(root_.get());
    }

    void run() {
        std::string command;
        std::cout << "Available commands: add_circle, add_rectangle, add_group, add_to_group, remove, move, move_to_group, copy, undo, redo, find, iterate, print, exit\n";
        while (true) {
            std::cout << "Enter command: ";
            if (!std::getline(std::cin, command)) {
                std::cout << "Input error or EOF detected. Exiting.\n";
                break;
            }
            std::istringstream iss(command);
            std::string cmd;
            if (!(iss >> cmd)) {
                std::cout << "Empty command. Try again.\n";
                continue;
            }
            if (cmd == "exit") break;
            else if (cmd == "add_circle") addCircleCommand(iss);
            else if (cmd == "add_rectangle") addRectangleCommand(iss);
            else if (cmd == "add_group") addGroupCommand(iss);
            else if (cmd == "add_to_group") addToGroupCommand(iss);
            else if (cmd == "remove") removeCommand(iss);
            else if (cmd == "move") moveCommand(iss);
            else if (cmd == "move_to_group") moveToGroupCommand(iss);
            else if (cmd == "copy") copyCommand(iss);
            else if (cmd == "undo") undoCommand();
            else if (cmd == "redo") redoCommand();
            else if (cmd == "find") findCommand(iss);
            else if (cmd == "iterate") iterateCommand();
            else if (cmd == "print") printCommand();
            else std::cout << "Unknown command. Available: add_circle, add_rectangle, add_group, add_to_group, remove, move, move_to_group, copy, undo, redo, find, iterate, print, exit\n";
        }
    }

private:
    std::unique_ptr<Group> root_;
    std::stack<std::unique_ptr<Command>> undoStack_;
    std::stack<std::unique_ptr<Command>> redoStack_;
    std::vector<Group*> groups_;

    void executeCommand(std::unique_ptr<Command> cmd) {
        cmd->execute();
        undoStack_.push(std::move(cmd));
        while (!redoStack_.empty()) redoStack_.pop();
    }

    Group* findParent(Group* current, Element* e) {
        for (auto& child : current->getElements()) {
            if (child.get() == e) return current;
            if (auto* g = dynamic_cast<Group*>(child.get())) {
                if (Group* parent = findParent(g, e)) return parent;
            }
        }
        return nullptr;
    }

    Group* findGroupByPosition(float x, float y) {
        for (Group* g : groups_) {
            if (std::abs(g->x - x) < 1e-5 && std::abs(g->y - y) < 1e-5) return g;
        }
        return nullptr;
    }

    void addCircle(float x, float y, float r) {
        auto circle = std::make_unique<Circle>(x, y, r);
        executeCommand(std::make_unique<AddCommand>(root_.get(), std::move(circle)));
        std::cout << "Added circle at (" << x << ", " << y << ") with radius " << r << "\n";
    }

    void addRectangle(float x, float y, float w, float h) {
        auto rect = std::make_unique<Rectangle>(x, y, w, h);
        executeCommand(std::make_unique<AddCommand>(root_.get(), std::move(rect)));
        std::cout << "Added rectangle at (" << x << ", " << y << ") with width " << w << " and height " << h << "\n";
    }

    Group* addGroup(float x, float y) {
        auto group = std::make_unique<Group>(x, y);
        Group* groupPtr = group.get();
        executeCommand(std::make_unique<AddCommand>(root_.get(), std::move(group)));
        groups_.push_back(groupPtr);
        std::cout << "Added group at (" << x << ", " << y << ")\n";
        return groupPtr;
    }

    void addToGroupCommand(std::istringstream& iss) {
        float group_x, group_y, x, y;
        std::string type;
        if (!(iss >> group_x >> group_y >> type >> x >> y)) {
            std::cout << "Invalid parameters for add_to_group (expected: group_x group_y type x y [params])\n";
            return;
        }
        Group* group = findGroupByPosition(group_x, group_y);
        if (!group) {
            std::cout << "No group found at (" << group_x << ", " << group_y << ")\n";
            return;
        }
        if (type == "circle") {
            float r;
            if (iss >> r) {
                auto circle = std::make_unique<Circle>(x, y, r);
                executeCommand(std::make_unique<AddCommand>(group, std::move(circle)));
                std::cout << "Added circle to group at (" << group_x << ", " << group_y << ")\n";
            }
            else {
                std::cout << "Invalid circle parameters (expected: r)\n";
            }
        }
        else if (type == "rectangle") {
            float w, h;
            if (iss >> w >> h) {
                auto rect = std::make_unique<Rectangle>(x, y, w, h);
                executeCommand(std::make_unique<AddCommand>(group, std::move(rect)));
                std::cout << "Added rectangle to group at (" << group_x << ", " << group_y << ")\n";
            }
            else {
                std::cout << "Invalid rectangle parameters (expected: w h)\n";
            }
        }
        else if (type == "group") {
            auto newGroup = std::make_unique<Group>(x, y);
            Group* newGroupPtr = newGroup.get();
            executeCommand(std::make_unique<AddCommand>(group, std::move(newGroup)));
            groups_.push_back(newGroupPtr);
            std::cout << "Added subgroup to group at (" << group_x << ", " << group_y << ")\n";
        }
        else {
            std::cout << "Invalid element type (expected: circle, rectangle, group)\n";
        }
    }

    void removeCommand(std::istringstream& iss) {
        float x, y;
        if (iss >> x >> y) {
            if (Element* e = findElement(x, y)) {
                removeElement(e);
                std::cout << "Removed element at (" << x << ", " << y << ")\n";
            }
            else {
                std::cout << "No element found at (" << x << ", " << y << ")\n";
            }
        }
        else {
            std::cout << "Invalid parameters for remove (expected: x y)\n";
        }
    }

    void moveCommand(std::istringstream& iss) {
        float x, y, dx, dy;
        if (iss >> x >> y >> dx >> dy) {
            if (Element* e = findElement(x, y)) {
                executeCommand(std::make_unique<MoveCommand>(e, dx, dy));
                std::cout << "Moved element to (" << e->x << ", " << e->y << ")\n";
            }
            else {
                std::cout << "No element found at (" << x << ", " << y << ")\n";
            }
        }
        else {
            std::cout << "Invalid parameters for move (expected: x y dx dy)\n";
        }
    }

    void moveToGroupCommand(std::istringstream& iss) {
        float element_x, element_y, group_x, group_y;
        if (iss >> element_x >> element_y >> group_x >> group_y) {
            Element* e = findElement(element_x, element_y);
            Group* target = (group_x == 0 && group_y == 0) ? root_.get() : findGroupByPosition(group_x, group_y);
            if (!e) {
                std::cout << "No element found at (" << element_x << ", " << element_y << ")\n";
                return;
            }
            if (!target) {
                std::cout << "No group found at (" << group_x << ", " << group_y << ")\n";
                return;
            }
            moveToGroup(e, target);
            std::cout << "Moved element from (" << element_x << ", " << element_y << ") to group at (" << group_x << ", " << group_y << ")\n";
        }
        else {
            std::cout << "Invalid parameters for move_to_group (expected: element_x element_y group_x group_y)\n";
        }
    }

    void copyCommand(std::istringstream& iss) {
        float x, y;
        if (iss >> x >> y) {
            if (Element* e = findElement(x, y)) {
                auto copied = copyElement(e);
                executeCommand(std::make_unique<AddCommand>(root_.get(), std::move(copied)));
                std::cout << "Copied element at (" << x << ", " << y << ")\n";
            }
            else {
                std::cout << "No element found at (" << x << ", " << y << ")\n";
            }
        }
        else {
            std::cout << "Invalid parameters for copy (expected: x y)\n";
        }
    }

    void undoCommand() {
        if (undoStack_.empty()) {
            std::cout << "Nothing to undo\n";
        }
        else {
            undo();
            std::cout << "Undone last operation\n";
        }
    }

    void redoCommand() {
        if (redoStack_.empty()) {
            std::cout << "Nothing to redo\n";
        }
        else {
            redo();
            std::cout << "Redone last operation\n";
        }
    }

    void findCommand(std::istringstream& iss) {
        float x, y;
        if (iss >> x >> y) {
            if (Element* e = findElement(x, y)) {
                std::cout << "Found element:\n";
                e->print();
            }
            else {
                std::cout << "No element found at (" << x << ", " << y << ")\n";
            }
        }
        else {
            std::cout << "Invalid parameters for find (expected: x y)\n";
        }
    }

    void iterateCommand() {
        std::cout << "Iterating over elements:\n";
        ElementIterator it = getIterator();
        while (it.hasNext()) {
            const Element* e = it.next();
            if (e) {
                e->print();
            }
            else {
                std::cout << "Warning: Iterator returned nullptr, skipping.\n";
            }
        }
    }

    void printCommand() {
        std::cout << "Document structure:\n";
        printDocument();
    }

    void addCircleCommand(std::istringstream& iss) {
        float x, y, r;
        if (iss >> x >> y >> r) {
            addCircle(x, y, r);
        }
        else {
            std::cout << "Invalid parameters for add_circle (expected: x y r)\n";
        }
    }

    void addRectangleCommand(std::istringstream& iss) {
        float x, y, w, h;
        if (iss >> x >> y >> w >> h) {
            addRectangle(x, y, w, h);
        }
        else {
            std::cout << "Invalid parameters for add_rectangle (expected: x y w h)\n";
        }
    }

    void addGroupCommand(std::istringstream& iss) {
        float x, y;
        if (iss >> x >> y) {
            addGroup(x, y);
        }
        else {
            std::cout << "Invalid parameters for add_group (expected: x y)\n";
        }
    }

    Element* findElement(float x, float y) {
        return root_->find(x, y);
    }
    void moveElement(Element* e, float dx, float dy) { if (e) e->move(dx, dy); }
    std::unique_ptr<Element> copyElement(const Element* e) { return e ? e->clone() : nullptr; }
    void undo() {
        if (!undoStack_.empty()) {
            undoStack_.top()->undo();
            redoStack_.push(std::move(undoStack_.top()));
            undoStack_.pop();
        }
    }
    void redo() {
        if (!redoStack_.empty()) {
            redoStack_.top()->execute();
            undoStack_.push(std::move(redoStack_.top()));
            redoStack_.pop();
        }
    }
    void printDocument() const { root_->print(); }
    ElementIterator getIterator() const { return ElementIterator(root_.get()); }
    void removeElement(Element* e) {
        if (e) {
            Group* parent = findParent(root_.get(), e);
            if (parent) {
                executeCommand(std::make_unique<RemoveCommand>(parent, e));
            }
        }
    }
    void moveToGroup(Element* e, Group* target) {
        if (e && target) {
            Group* source = findParent(root_.get(), e);
            if (source) {
                executeCommand(std::make_unique<MoveToGroupCommand>(source, target, e));
            }
        }
    }
};

// Entry point
int main() {
    Editor editor;
    editor.run();
    return 0;
}