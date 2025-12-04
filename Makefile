BPF_CLANG ?= clang
BPF_LLVM ?= llc
TARGET = bpf/mlfq.bpf.o
SRC = bpf/mlfq.bpf.c
INCLUDE = -I. -Iinclude

all: $(TARGET)

$(TARGET): $(SRC)
	$(BPF_CLANG) -O2 -target bpf -g $(SRC) $(INCLUDE) -o $(TARGET)

clean:
	rm -f $(TARGET)
