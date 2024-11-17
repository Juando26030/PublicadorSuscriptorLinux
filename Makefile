# Compilador y banderas
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

# Archivos fuente
PUB_SRC = publicador.c
SUB_SRC = suscriptor.c
SYS_SRC = sistema.c

# Ejecutables
PUB_EXEC = publicador
SUB_EXEC = suscriptor
SYS_EXEC = sistema

# Objetivo principal: compilar todo
all: $(PUB_EXEC) $(SUB_EXEC) $(SYS_EXEC)

# Compilación del publicador
$(PUB_EXEC): $(PUB_SRC)
	$(CC) $(CFLAGS) -o $(PUB_EXEC) $(PUB_SRC)

# Compilación del suscriptor
$(SUB_EXEC): $(SUB_SRC)
	$(CC) $(CFLAGS) -o $(SUB_EXEC) $(SUB_SRC)

# Compilación del sistema de comunicación
$(SYS_EXEC): $(SYS_SRC)
	$(CC) $(CFLAGS) -o $(SYS_EXEC) $(SYS_SRC)

# Limpieza de ejecutables
clean:
	rm -f $(PUB_EXEC) $(SUB_EXEC) $(SYS_EXEC)

# Limpieza completa (ejecutables y pipes nominales)
full_clean: clean
	rm -f pipePSC pipeSSC
