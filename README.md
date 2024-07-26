# llm.c

LLMs in simple, pure C/OpenCL with no need for 245MB of PyTorch or 107MB of cPython. Current focus is on pretraining, in particular reproducing the [GPT-2](https://github.com/openai/gpt-2) and [GPT-3](https://arxiv.org/abs/2005.14165) miniseries, along with a parallel PyTorch reference implementation in [train_gpt2.py](train_gpt2.py). You'll recognize this file as a slightly tweaked [nanoGPT](https://github.com/karpathy/nanoGPT), an earlier project of mine. Currently, llm.c is a bit faster than PyTorch Nightly (by about 7%). We have a simple reference CPU fp32 implementation in ~1,000 lines of clean code in one file [train_gpt2.c](train_gpt2.c). Ports to other languages or repos are very welcome, but should be done in separate repos, and I am happy to link to them below in the "notable forks" section. Developer coordination happens in the [Discussions](https://github.com/karpathy/llm.c/discussions) and on Discord, either the `#llmc` channel on the [Zero to Hero](https://discord.gg/3zy8kqD9Cp) channel.

## quick start

The best introduction to the llm.c repo today is reproducing the GPT-2 (124M) model. [Discussion #481](https://github.com/karpathy/llm.c/discussions/481) steps through this in detail. We can reproduce other models from the GPT-2 and GPT-3 series in both llm.c and in the parallel implementation of PyTorch. Have a look at the [scripts README](scripts/README.md).

debugging tip: when you run the `make` command to build the binary, modify it by replacing `-O3` with `-g` so you can step through the code in your favorite IDE (e.g. vscode).

## quick start (OpenCL, 1 GPU, fp32 only)

This port is a reference implementation using OpenCL. Only matmul routine is ported to OpenCL for now. The kernels have some tuneable parameters, and the default values won't perform well on most devices. So, a separate tuner is implemented which finds the most efficient kernel parameters for your device.

```bash
pip install -r requirements.txt
python dev/data/tinyshakespeare.py
python train_gpt2.py
make train_gpt2cl
./train_gpt2cl
```

Check the correctness of the kernels by running the unit tests:

```bash
make test_gpt2cl
./test_gpt2cl
```

All the executables accept parameters via environment variables.

* CL_PLATFORM_IDX - Select the OpenCL platform index. Default is 0.
* CL_DEVICE_IDX - Select the OpenCL device index. Default is 0.

Tuneable prameters:

* MATMUL_TILE_SIZE - The tile size for the matmul kernel.
* MATMUL_LOCAL_MEM_PADDING_SIZE - The local memory padding size for the matmul kernel.
* MATMUL_VLOAD_SIZE - The vload size for the matmul kernel.
* MATMUL_DO_PRELOAD - Set it to 1 to enable preload.
* MATMUL_USE_MAD - Set it to 1 to use mad instruction.

### Tuner
Tuner goes through a combination of above parameters and finds the best combination for the device. It takes a while to run.

```bash
make tune_gpt2cl
./tune_gpt2cl
```

Prints out the best combination of parameters for the device, for example:

```MATMUL_TILE_SIZE=16 MATMUL_LOCAL_MEM_PADDING_SIZE=1 MATMUL_VLOAD_SIZE=8 MATMUL_DO_PRELOAD=1 MATMUL_USE_MAD=1```

It can be used to launch the subsequent run:

```bash
MATMUL_TILE_SIZE=16 MATMUL_LOCAL_MEM_PADDING_SIZE=1 MATMUL_VLOAD_SIZE=8 MATMUL_DO_PRELOAD=1 MATMUL_USE_MAD=1 ./train_gpt2cl
```

## quick start (CPU)

The "I am so GPU poor that I don't even have one GPU" section. You can still enjoy seeing llm.c train! But you won't go too far. Just like the fp32 version above, the CPU version is an even earlier checkpoint in the history of llm.c, back when it was just a simple reference implementation in C. For example, instead of training from scratch, you can finetune a GPT-2 small (124M) to output Shakespeare-like text, as an example:

```bash
chmod u+x ./dev/download_starter_pack.sh
./dev/download_starter_pack.sh
make train_gpt2
OMP_NUM_THREADS=8 ./train_gpt2
```

If you'd prefer to avoid running the starter pack script, then as mentioned in the previous section you can reproduce the exact same .bin files and artifacts by running `python dev/data/tinyshakespeare.py` and then `python train_gpt2.py`.

The above lines (1) download an already tokenized [tinyshakespeare](https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt) dataset and download the GPT-2 (124M) weights, (3) init from them in C and train for 40 steps on tineshakespeare with AdamW (using batch size 4, context length only 64), evaluate validation loss, and sample some text. Honestly, unless you have a beefy CPU (and can crank up the number of OMP threads in the launch command), you're not going to get that far on CPU training LLMs, but it might be a good demo/reference. The output looks like this on my MacBook Pro (Apple Silicon M3 Max):

```
[GPT-2]
max_seq_len: 1024
vocab_size: 50257
num_layers: 12
num_heads: 12
channels: 768
num_parameters: 124439808
train dataset num_batches: 1192
val dataset num_batches: 128
num_activations: 73323776
val loss 5.252026
step 0: train loss 5.356189 (took 1452.121000 ms)
step 1: train loss 4.301069 (took 1288.673000 ms)
step 2: train loss 4.623322 (took 1369.394000 ms)
step 3: train loss 4.600470 (took 1290.761000 ms)
... (trunctated) ...
step 39: train loss 3.970751 (took 1323.779000 ms)
val loss 4.107781
generating:
---
Come Running Away,
Greater conquer
With the Imperial blood
the heaviest host of the gods
into this wondrous world beyond.
I will not back thee, for how sweet after birth
Netflix against repounder,
will not
flourish against the earlocks of
Allay
---
```

## datasets

The data files inside `/dev/data/(dataset).py` are responsible for downloading, tokenizing and saving the tokens to .bin files, readable easily from C. So for example when you run:

```bash
python dev/data/tinyshakespeare.py
```

We download and tokenize the [tinyshakespeare](https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt) dataset. The output of this looks like this:

```
writing 32,768 tokens to ./dev/data/tinyshakespeare/tiny_shakespeare_val.bin
writing 305,260 tokens to ./dev/data/tinyshakespeare/tiny_shakespeare_train.bin
```

The .bin files contain a short header (1024 bytes) and then a stream of tokens in uint16, indicating the token ids with the GPT-2 tokenizer. More datasets are available in `/dev/data`.

## test

I am also attaching a simple unit test for making sure our C code agrees with the PyTorch code. On the CPU as an example, compile and run with:

```bash
make test_gpt2
./test_gpt2
```

This now loads the `gpt2_124M_debug_state.bin` file that gets written by train_gpt2.py, runs a forward pass, compares the logits and loss with the PyTorch reference implementation, then it does 10 iterations of training with Adam and makes sure the losses match PyTorch. To test the GPU version we run:

```bash
# fp32 test (cudnn not supported)
make test_gpt2cu PRECISION=FP32 && ./test_gpt2cu
# mixed precision cudnn test
make test_gpt2cu USE_CUDNN=1 && ./test_gpt2cu
```

This tests both the fp32 path and the mixed precision path. The test should pass and print `overall okay: 1`.

## tutorial

I attached a very small tutorial here, in [doc/layernorm/layernorm.md](doc/layernorm/layernorm.md). It's a simple, step-by-step guide to implementing a single layer of the GPT-2 model, the layernorm layer. This is a good starting point to understand how the layers are implemented in C.

**flash attention**. As of May 1, 2024 we use the Flash Attention from cuDNN. Because cuDNN bloats the compile time from a few seconds to ~minute and this code path is right now very new, this is disabled by default. You can enable it by compiling like this:

```bash
make train_gpt2cu USE_CUDNN=1
```

This will try to compile with cudnn and run it. You have to have cuDNN installed on your system. The [cuDNN installation instructions](https://developer.nvidia.com/cudnn) with apt-get will grab the default set of cuDNN packages. For a minimal setup, the cuDNN dev package is sufficient, e.g. on Ubuntu 22.04 for CUDA 12.x:

```bash
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt-get update
sudo apt-get -y install libcudnn9-dev-cuda-12
```

On top of this you need the [cuDNN frontend](https://github.com/NVIDIA/cudnn-frontend/tree/main), but this is just header files. Simply clone the repo to your disk. The Makefile currently looks for it in either your home directory or the current directory. If you have put it elsewhere, add `CUDNN_FRONTEND_PATH=/path/to/your/cudnn-frontend/include` to the `make` command-line.

## repo

A few more words on what I want this repo to be:

First, I want `llm.c` to be a place for education. E.g. our `dev/cuda` folder is a place for a library of kernels for all the layers that are manually hand-written and very well documented, starting from very simple kernels all the way to more complex / faster kernels. If you have a new kernel with various different tradeoffs, please feel free to contribute it here.

That said, I also want `llm.c` to be very fast too, even practically useful to train networks. E.g. to start, we should be able to reproduce the big GPT-2 (1.6B) training run. This requires that we incorporate whatever fastest kernels there are, including the use of libraries such as cuBLAS, cuBLASLt, CUTLASS, cuDNN, etc. I also think doing so serves an educational purpose to establish an expert upper bound, and a unit of measurement, e.g. you could say that your manually written kernels are 80% of cuBLAS speed, etc. Then you can choose to do a super fast run, or you can choose to "drag and drop" whatever manual kernels you wish to use, and run with those.

However, as a constraint, I want to keep the mainline `llm.c` in the root folder simple and readable. If there is a PR that e.g. improves performance by 2% but it "costs" 500 lines of complex C code, and maybe an exotic 3rd party dependency, I may reject the PR because the complexity is not worth it. As a concrete example - making cuBLAS for matmuls the default in the root training loop is a no-brainer: it makes the mainline code much faster, it is a single line of interpretable code, and it is a very common dependency. On the side of this, we can have manual implementations that can compete with cuBLAS in `dev/cuda`.

Lastly, I will be a lot more sensitive to complexity in the root folder of the project, which contains the main / default files of the project. In comparison, the `dev/` folder is a bit more of a scratch space for us to develop a library of kernels or classes and share useful or related or educational code, and some of this code could be ok to be (locally) complex.

## notable forks

- AMD support
  - [llm.c](https://github.com/anthonix/llm.c) by @[anthonix](https://github.com/anthonix): support for AMD devices, such as the 7900 XTX

- C#
  - [llm.cs](https://github.com/azret/llm.cs) by @[azret](https://github.com/azret): a C# port of this project
  - [Llm.cs](https://github.com/nietras/Llm.cs) by @[nietras](https://github.com/nietras): a C# port of this project with focus on easy to get started on any platform. Clone and run ✅

- CUDA C++
  - [llm.cpp](https://github.com/gevtushenko/llm.c) by @[gevtushenko](https://github.com/gevtushenko): a port of this project using the [CUDA C++ Core Libraries](https://github.com/NVIDIA/cccl)
     - A presentation this fork was covered in [this lecture](https://www.youtube.com/watch?v=WiB_3Csfj_Q) in the [CUDA MODE Discord Server](https://discord.gg/cudamode)

- WebGPU C++
  - [gpu.cpp](https://github.com/AnswerDotAI/gpu.cpp) by @[austinvhuang](https://github.com/austinvhuang): a library for portable GPU compute in C++ using native WebGPU. Aims to be a general-purpose library, but also porting llm.c kernels to WGSL.

- Go
  - [llm.go](https://github.com/joshcarp/llm.go) by @[joshcarp](https://github.com/joshcarp): a Go port of this project

- Java
  - [llm.java](https://github.com/harryjackson/llm.java) by @[harryjackson](https://github.com/harryjackson): a Java port of this project

- Metal
  - [llm.metal](https://github.com/regrettable-username/llm.metal) by @[regrettable-username](https://github.com/regrettable-username): LLM training in simple, raw C/Metal Shading Language

- Mojo
  - [llm.🔥](https://github.com/dorjeduck/llm.mojo) by @[dorjeduck](https://github.com/dorjeduck): a Mojo port of this project

- OpenCL
  - [llm.c](https://github.com/krrishnarraj/llm.c) by @[krrishnarraj](https://github.com/krrishnarraj): an OpenCL port of this project

- Rust
  -  [llm.rs](https://github.com/yijunyu/llm.rs) by @[Yijun Yu](https://github.com/yijunyu): a Rust rewrite with the aim to have same performance
  -  [llm.rs](https://github.com/ToJen/llm.rs) by @[ToJen](https://github.com/ToJen): a Rust port of this project

- Swift
  - [llm.swift](https://github.com/otabuzzman/llm.swift) by @[otabuzzman](https://github.com/otabuzzman): a Swift port of this project

- Zig
  - [llm.zig](https://github.com/Saimirbaci/llm.zig) by @[saimirbaci](https://github.com/Saimirbaci): a Zig port of this project
 
- Habana Gaudi2
  - [llm.tpc](https://github.com/abhilash1910/llm.tpc) by @[abhilash1910](https://github.com/abhilash1910): a Habana Gaudi2 port of this project 

- Nim
  - [llm.nim](https://github.com/Vindaar/llm.nim) by @[Vindaar](https://github.com/Vindaar): a Nim port of this project

## discussions

Ways of organizing development:

- Experiencing a concrete issue with the repo? Use [Issues](https://github.com/karpathy/llm.c/issues).
- Have some code to contribute? Open a [PR](https://github.com/karpathy/llm.c/pulls)
- Chat about the repo, ask questions, etc.? Look at [Discussions](https://github.com/karpathy/llm.c/discussions).
- Something faster? I created a new `#llmc` channel on my [Zero to Hero Discord channel](https://discord.gg/3zy8kqD9Cp).

## license

MIT
