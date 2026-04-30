#include <iostream>
#include "weft/task.h"

int main() {
    std::cout << "Weft: Work-Stealing Thread Pool\n";
    std::cout << "Bootstrapping Day 1...\n";
    
    weft::Task sample_task = []() {
        std::cout << "Executing a sample task!\n";
    };
    
    sample_task();
    
    return 0;
}
