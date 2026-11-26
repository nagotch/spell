# Rust 版（cargo）のビルドを、C 版と同じ配置（src/spell と
# src/spell-lsp が prelude.sp の隣に置かれる）へつなぐ shim。
# tests/test.sh は `make -C ../src` でこれを呼ぶ。
CARGO ?= cargo

all: spell spell-lsp

spell spell-lsp: cargo-build
	cp target/release/spell spell
	cp target/release/spell-lsp spell-lsp

cargo-build:
	$(CARGO) build --release --offline --locked

test: spell
	./test.sh

clean:
	rm -f spell spell-lsp
	$(CARGO) clean

.PHONY: all cargo-build test clean
