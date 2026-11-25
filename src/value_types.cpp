#include "elevator/value_types.h"

#include "elevator/exceptions.h"

std::string toString(Direction d) {
    switch (d) {
        case Direction::Up: return "Lên";
        case Direction::Down: return "Xuống";
        case Direction::None: return "Đứng yên";
    }
    return "?";
}

std::string toToken(Direction d) {
    switch (d) {
        case Direction::Up: return "up";
        case Direction::Down: return "down";
        case Direction::None: return "none";
    }
    return "none";
}

Direction parseDirection(const std::string& text) {
    std::string t = text;
    for (char& ch : t)
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';

    if (t == "up") return Direction::Up;
    if (t == "down") return Direction::Down;
    if (t == "none") return Direction::None;
    throw ValidationError("Hướng không hợp lệ: '" + text + "' (dùng up/down)");
}

std::ostream& operator<<(std::ostream& os, Direction d) { return os << toString(d); }

int floorDistance(int a, int b) { return a > b ? a - b : b - a; }

Weight::Weight(double kg) : kg_(kg) {
    if (kg < 0.0) throw ValidationError("Khối lượng không hợp lệ: " + kgText(kg) + " kg");
}

Weight Weight::operator+(const Weight& other) const { return Weight(kg_ + other.kg_); }

Weight Weight::operator-(const Weight& other) const {
    if (other.kg_ > kg_) throw ValidationError("Khối lượng sau khi trừ bị âm");
    return Weight(kg_ - other.kg_);
}

Weight Weight::operator*(double k) const { return Weight(kg_ * k); }

Weight& Weight::operator+=(const Weight& other) {
    kg_ += other.kg_;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const Weight& w) { return os << kgText(w.kg()) << " kg"; }

Direction Passenger::direction() const { return destination > origin ? Direction::Up : Direction::Down; }

std::string Passenger::describe() const {
    return "#" + std::to_string(id) + " (" + kgText(weight.kg()) + " kg): tầng " + std::to_string(origin) +
           " -> tầng " + std::to_string(destination);
}
