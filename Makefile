# Cabinet Guardian
#
#   make test    编译并运行算法层验证用例
#   make clean   清理临时产物

SHELL := /bin/bash
TEST  := code/pc_test/build_run.sh

.PHONY: all test clean

all: test

test:
	@echo "== 编译并运行算法层验证 =="
	@bash $(TEST)

clean:
	@rm -rf /tmp/cabinet_test_*
	@echo "已清理"
