all: publicador suscriptor sistema

publicador: publicador.c
	gcc -o publicador publicador.c

suscriptor: suscriptor.c
	gcc -o suscriptor suscriptor.c

sistema: sistema.c
	gcc -o sistema sistema.c

clean:
	rm -f publicador suscriptor sistema