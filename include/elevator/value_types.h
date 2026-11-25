#pragma once
#include <iostream>
#include <string>

enum class Direction { Up, Down, None };

std::string toString(Direction d);
std::string toToken(Direction d);
Direction parseDirection(const std::string& text);
std::ostream& operator<<(std::ostream& os, Direction d);

int floorDistance(int a, int b);

class Weight {
public:
    Weight() : kg_(0.0) {}
    explicit Weight(double kg);

    double kg() const { return kg_; }

    Weight operator+(const Weight& other) const;
    Weight operator-(const Weight& other) const;
    Weight operator*(double k) const;
    Weight& operator+=(const Weight& other);

    bool operator==(const Weight& other) const { return kg_ == other.kg_; }
    bool operator<(const Weight& other) const { return kg_ < other.kg_; }
    bool operator>(const Weight& other) const { return kg_ > other.kg_; }
    bool operator<=(const Weight& other) const { return kg_ <= other.kg_; }
    bool operator>=(const Weight& other) const { return kg_ >= other.kg_; }

private:
    double kg_;
};

std::ostream& operator<<(std::ostream& os, const Weight& w);

struct Passenger {
    int id = 0;
    Weight weight;
    int origin = 0;
    int destination = 0;
    long arrivalTick = 0;
    long boardTick = -1;

    Direction direction() const;
    std::string describe() const;
};
