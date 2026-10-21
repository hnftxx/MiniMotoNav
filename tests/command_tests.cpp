#include "app/moto_command.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace moto;

namespace {

void test_parse_commands() {
    Command c;
    assert(parse_command("mode nav", c));
    assert(c.type == CommandType::SetMode);
    assert(c.mode == AppMode::Nav);

    assert(parse_command("theme night", c));
    assert(c.type == CommandType::SetTheme);
    assert(c.theme == 2);

    assert(parse_command("target 400", c));
    assert(c.type == CommandType::SetDragTarget);
    assert(std::fabs(c.value - 400.0f) < 0.01f);

    assert(parse_command("split 200.5", c));
    assert(c.type == CommandType::SetDragSplit);
    assert(std::fabs(c.value - 200.5f) < 0.01f);

    assert(parse_command("countdown 3000", c));
    assert(c.type == CommandType::SetDragCountdown);
    assert(std::fabs(c.value - 3000.0f) < 0.01f);

    assert(parse_command("speed 80", c));
    assert(c.type == CommandType::AdjustSpeed);

    assert(parse_command("n", c));
    assert(c.type == CommandType::NextMode);

    assert(parse_command("drag", c));
    assert(c.type == CommandType::DragRun);

    assert(!parse_command("", c));
    assert(!parse_command("bogus", c));
    assert(!parse_command("mode nowhere", c));
    assert(!parse_command("target", c));
}

void test_queue_fifo() {
    CommandQueue q;
    assert(q.empty());

    Command a;
    a.type = CommandType::NextMode;
    Command b;
    b.type = CommandType::ToggleFps;
    assert(q.post(a));
    assert(q.post(b));
    assert(q.count() == 2);

    Command out;
    assert(q.pop(out));
    assert(out.type == CommandType::NextMode);
    assert(q.pop(out));
    assert(out.type == CommandType::ToggleFps);
    assert(!q.pop(out));
    assert(q.empty());
}

void test_queue_capacity() {
    CommandQueue q;
    Command c;
    c.type = CommandType::NextMode;
    for (int i = 0; i < CommandQueue::kCapacity; ++i) assert(q.post(c));
    assert(!q.post(c));
    assert(q.count() == CommandQueue::kCapacity);

    Command out;
    assert(q.pop(out));
    assert(q.post(c));
    assert(q.count() == CommandQueue::kCapacity);
}

} // namespace

int main() {
    test_parse_commands();
    test_queue_fifo();
    test_queue_capacity();
    std::puts("command_tests: ok");
    return 0;
}
