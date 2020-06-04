default:
	gcc -Wall src/ast.c src/buffer.c src/lexer.c src/parser.c src/stack.c src/symbol.c src/main.c -o builds/compiler