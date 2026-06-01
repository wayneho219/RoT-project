# 用法：
#   make run FILE=module-01/ex01    # 編譯並執行
#   make check FILE=module-01/ex01  # 執行並比對預期輸出
#   make clean                      # 清除編譯產物

CC = clang
CFLAGS = -Wall -Wextra -std=c99 -g

run: $(FILE).c
	$(CC) $(CFLAGS) -o $(FILE).out $(FILE).c && ./$(FILE).out

check: $(FILE).c
	$(CC) $(CFLAGS) -o $(FILE).out $(FILE).c && \
	./$(FILE).out > /tmp/actual.txt && \
	diff $(FILE).expected /tmp/actual.txt \
		&& echo "✓ 通過" || echo "✗ 輸出不符，請檢查"

clean:
	find . -name "*.out" -delete
