#include <iostream>
#include <random>
#include <cstdlib>
#include <thread>
#include <vector>
#include <functional>

struct S
{
    virtual void f1() = 0;
};

struct S1 : S
{
    void f1() override { i++; }
    void f2() { i++;  }
    alignas(128) volatile int i = 0;
} s1;

struct S2 : S
{
    void f1() override { j--; }
    void f2() { j--; }
    alignas(128) volatile int j = 0;
} s2;

S* s[2] = { &s1, &s2 };

std::vector<int> random_data;

template<bool Virtual, bool Mispredict>
std::chrono::duration<double> Run()
{
    auto t = std::chrono::steady_clock::now();

    for (std::size_t n = 0; n != 100'000; n++)
    {
        for (std::size_t i = 0, m = random_data.size(); i != m; i++)
        {
            bool branch = (Mispredict ? 
                random_data[i] & 1  // equal probablilty - predicted poorly
                : 
                random_data[i] == -1 // impossible true - predicted well
                );

            if (Virtual)
            {
                s[branch]->f1();
            }
            else
            {
                branch ? s1.f2() : s2.f2();
            }
        }
    }    

    return std::chrono::steady_clock::now() - t;    
}

int main()
{
    // Added vtable fake rewrite
    std::thread([]
        {
            volatile std::intptr_t* v0 = *((volatile std::intptr_t**)(&s[0]));
            volatile std::intptr_t* v1 = *((volatile std::intptr_t**)(&s[1]));
            for (;;)
            {
                *v0 |= 0;
                *v1 |= 0;
            }

        }).detach();

    std::size_t random_numbers = 1'000;
    random_data.reserve(random_numbers);
    std::generate_n(std::back_inserter(random_data),
        random_numbers, 
        std::bind(
            std::uniform_int_distribution<>{},
            std::default_random_engine{ std::random_device{}() }));

    std::cout << Run<false, false>().count() << "s nonvirtual predicted\n";
    std::cout << Run<false, true >().count() << "s nonvirtual mispredicted\n";
    std::cout << Run<true,  false>().count() << "s virtual predicted\n";
    std::cout << Run<true,  true >().count() << "s virtual mispredicted\n";
}
