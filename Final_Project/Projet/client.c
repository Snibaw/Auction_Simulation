// client.c
#include "pse.h"

#define CMD "client"

void *lireServeur(void *arg)
{
  int fin = FAUX;
  char ligne[LIGNE_MAX];
  int *sock = (int *)arg;
  while (!fin)
  {
    if (lireLigne(*sock, ligne) > 0)
    {
      printf("%s\n", ligne);
    }
    else
    {
      printf("fin");
      fin = 1;
    }
  }

  if (close(*sock) == -1)
    erreur_IO("fermeture canal");
  pthread_exit(NULL);
}

void *ecrireServeur(void *arg)
{
  int fin = FAUX;
  int lgEcr;
  char ligne[LIGNE_MAX];
  int *sock = (int *)arg;
  while (!fin)
  {
    if (fgets(ligne, LIGNE_MAX, stdin) == NULL) // saisie de CTRL-D
      fin = VRAI;
    else
    {
      lgEcr = ecrireLigne(*sock, ligne);
      if (lgEcr == -1)
        erreur_IO("ecriture socket");

      if (strcmp(ligne, "fin\n") == 0) // ecrireLigne a ajoute \n
        fin = VRAI;
    }
  }
  pthread_exit(NULL);
}
int main(int argc, char *argv[])
{
  int sock, ret;
  struct sockaddr_in *adrServ;

  // lorsque le client ecrit dans le socket alors que le serveur a ete arrete
  // il recoit le signal SIGPIPE qui par defaut met fin brutalement au
  // processus sans message d'erreur;
  // avec l'instruction qui suit on ignore ce signal, ecrireLigne sortira
  // en echec et le client affichera un message d'erreur
  signal(SIGPIPE, SIG_IGN);

  if (argc != 3)
    erreur("usage: %s machine port\n", argv[0]);

  printf("%s: creating a socket\n", CMD);
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    erreur_IO("socket");

  printf("%s: DNS resolving for %s, port %s\n", CMD, argv[1], argv[2]);
  adrServ = resolv(argv[1], argv[2]);
  if (adrServ == NULL)
    erreur("adresse %s port %s inconnus\n", argv[1], argv[2]);

  printf("%s: adr %s, port %hu\n", CMD,
         stringIP(ntohl(adrServ->sin_addr.s_addr)),
         ntohs(adrServ->sin_port));

  printf("%s: connecting the socket\n", CMD);
  ret = connect(sock, (struct sockaddr *)adrServ, sizeof(struct sockaddr_in));
  if (ret < 0)
    erreur_IO("connect");
  pthread_t idThread1, idThread2;
  pthread_create(&idThread1, NULL, lireServeur, &sock);
  pthread_create(&idThread2, NULL, ecrireServeur, &sock);
  while (1)
  {
  }
  if (close(sock) == -1)
    erreur_IO("fermeture socket");

  exit(EXIT_SUCCESS);
}
