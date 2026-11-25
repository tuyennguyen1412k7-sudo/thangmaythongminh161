#pragma once

class Door {
public:
    void open() {
        open_ = true;
        openTicks_ = 0;
    }
    void close() { open_ = false; }
    void tick() {
        if (open_) ++openTicks_;
    }

    bool isOpen() const { return open_; }
    int openTicks() const { return openTicks_; }

private:
    bool open_ = false;
    int openTicks_ = 0;
};
