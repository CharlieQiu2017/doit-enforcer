# Data Operand Independent Timing (DOIT) Enforcer

## What is this?

This is a tool for detecting potential timing side channels of x86 cryptographic applications.
It traces all executed instructions and all memory accesses of the program.
The instructions are checked against the [Data Operand Independent Timing (DOIT) ISA](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/resources/data-operand-independent-timing-instructions.html) published by Intel.
By comparing the traces of a program under different secret inputs,
one can verify that the program does not have any secret-dependent control flow.
This is good indication that the application does not have timing side channels.

Note 1: Timing side channels are not the only kind of side channels.
There are also power, frequency, and electromagnetic side channels.
However, non-timing side channels typically require local access to the device running the application to exploit.
Eliminating all timing side channels is usually sufficient for public x86 server applications.
For an interesting exception, see [Hertzbleed](https://www.hertzbleed.com/).

Note 2: According to the DOIT ISA document, the listed instructions are guaranteed not to exhibit timing side-channels only when the MSR named `IA32_UARCH_MISC_CTL[DOITM]` is set to `1`.
However, in a [Linux kernel mailing list message](https://lore.kernel.org/lkml/851920c5-31c9-ddd9-3e2d-57d379aa0671@intel.com/),
an Intel engineer has explained that these instructions are always constant-time,
and the MSR only controls prefetching behaviors.

Note 3: Intel has so far published two documents related to constant-time instructions.
Besides the DOIT ISA mentioned above, it has also documented certain families of processors exhibiting [MXCSR-dependent timing behaviors](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/resources/mcdt-data-operand-independent-timing-instructions.html).
Our tool does not currently check applications against this instruction list yet,
but can be easily modified to add this check should that be desirable.
AMD has said nothing so far regarding cryptographic side-channels.
We shall simply assume that instructions safe on Intel cores are also safe on AMD cores.

Note 4: This tool is based on [Intel Pin](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html).
The basic idea comes from the paper [DATA – Differential Address Trace Analysis: Finding Address-based Side-Channels in Binaries](https://www.usenix.org/conference/usenixsecurity18/presentation/weiser).
We greatly simplified their implementation, and added the feature of checking instructions against the DOIT ISA.

Note 5: Currently, Intel Pin only supports instrumenting x86 and x86-64 applications.
Consequently, our tool cannot be used on ARM programs.
Porting our tool to other instrumentation frameworks such as [DynamoRIO](https://dynamorio.org/) is future work.

## How to use this tool?

All instructions below are based on Ubuntu 22.04.

First download Intel Pin (https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html).
Extract it somewhere. Henceforth we use `$PIN` to represent the directory where the `pin` executable is located.

Build our tool with the command
```
mkdir obj
make PIN_ROOT=$PIN OBJDIR=./obj ./obj/doit-enforcer.so
```

We expect the cryptographic algorithm to be implemented in a "freestanding" style.
That is, it does not rely on any part of the C library, especially dynamic memory allocation.
To facilitate writing such programs,
we provide a minimal C library that only provides three functionalities: `read`, `write`, and `exit`.
This is sufficient to build an executable that reads some secret input from stdin,
runs some cryptographic algorithm, and writes the output to stdout.

As an example, suppose that your "algorithm" takes two integers and adds them together.
You first implement your algorithm in C:
```c
#include <stdint.h>

void crypto (uint32_t* in, uint32_t* out) {
    *out = in[0] + in[1];
}
```

Compile this file with `gcc -ffreestanding crypto.c -c -o crypto.o`,
and you get an object file `crypto.o` that exports the `crypto` function which is your implementation.

Next, we write a main function that reads input, calls this function, and writes output:
```c
#include <stdint.h>
#include "syscall.h"

uint32_t in[2], out;

void main (int argc, char** argv) {
    read (0, in, 8);
    crypto (in, &out);
    write (1, out, 4);
    exit (0);
}
```

Compile this file with `gcc -ffreestanding main.c -c -o main.o`.

We also need some startup code. This is provided in `crt.asm`. Compile it with `as crt.asm -o crt.o`.

Finally, link all objects together with `ld crypto.o main.o crt.o -e _start -o crypto`.

Now we are ready to invoke the pintool.
The command is `setarch -R $PIN/pin -t ./obj/doit-enforcer.so -- ./crypto < input_file > output_file`.
The `setarch` part is to disable address layout randomization, which causes the stack address to be randomized.

After execution, a trace file `trace.txt` is generated.
Each line of the trace represents one executed instruction.
The line begins with the address of the instruction. Then the effective address (EA) of each memory operand is listed.

If the tool detects an instruction that is not on the DOIT list, a warning is issued,
except for the following instructions for which no warning is issued:
* Unconditional jumps, including `CALL`, `RET`, and `LEAVE`;
* Conditional jumps;
* NOPs;
* `POPCNT` (see below).

To verify that the implementation does not have secret-dependent control flow,
first generate two random input files:
```bash
dd if=/dev/urandom of=input1.bin bs=1 count=8
dd if=/dev/urandom of=input2.bin bs=1 count=8
```

Then run the application under each input:
```bash
setarch -R $PIN/pin -t ./obj/doit-enforcer.so -- ./crypto < input1.bin > output1.bin
mv trace.txt trace1.txt
setarch -R $PIN/pin -t ./obj/doit-enforcer.so -- ./crypto < input2.bin > output2.bin
mv trace.txt trace2.txt
```

Finally, use `diff trace1.txt trace2.txt` to check that the execution flows are identical.

## Note on POPCNT instruction

The `POPCNT` instruction counts the number of bits set to 1 within an integer.
It is useful for implementing Merkle trees.
However, the Intel DOIT ISA does not list it as safe to use.
Therefore if this instruction appears within cryptographic applications,
one should be prepared that it leaks its operand.

Nevertheless, in many algorithms `POPCNT` is used such that its operand is a constant independent of secret inputs.
Therefore, we suppresed warnings against the use of this instruction.
However, whenever it is used we print the value of its operand in the trace,
so that if it is accidentally used with secret-dependent operand our tool will catch it.

## Interesting observations

The [reference implementation](https://github.com/BLAKE3-team/BLAKE3) of the BLAKE3 hash function (latest commit `d94882d` as of writing) contains a use of `SHUFPS`.
(https://github.com/BLAKE3-team/BLAKE3/blob/d94882d66d060fb035b3cb7199011b6577bb99ee/c/blake3_avx512.c#L5)

However, `SHUFPS` is not listed in Intel's DOIT ISA, leading to a potential side-channel.
To workaround this one can patch the offending macro
```c
#define _mm_shuffle_ps2(a, b, c)                                               \
  (_mm_castps_si128(                                                           \
      _mm_shuffle_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), (c))))
```
with
```c
#define _mm_shuffle_ps2(a, b, c) \
      _mm_blend_epi32 (_mm_shuffle_epi32((a), (c)), _mm_shuffle_epi32((b), (c)), 0b1100)
```