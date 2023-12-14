#ifndef ARCH_X86_64_PIT_H_
#define ARCH_X86_64_PIT_H_

void pit_init();
void pit_set_frequency(int hz);
void pit_one_shot(int ms);

#endif // ARCH_X86_64_PIT_H_
