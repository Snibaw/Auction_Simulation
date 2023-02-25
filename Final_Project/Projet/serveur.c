// serveur.c
#include "pse.h"
#include <ctype.h>

#define CMD "serveur"
#define NOM_JOURNAL "journal.log"
#define NB_WORKERS 10
#define NB_OBJET 6
typedef struct _objet
{
  int valeurBase;
  int id;
  char nom[50];
  int idAcheteur;
} objet;

objet ListeObjet[NB_OBJET];
typedef struct joueur
{
  int canal;
  int id;
  char nom[50];
  int nom_attribue;
} Joueur;

void init_obj(void)
{

  ListeObjet[0].valeurBase = 100;
  ListeObjet[0].id = 0;
  strcpy(ListeObjet[0].nom, "Vase");
  ListeObjet[0].idAcheteur = -1;

  ListeObjet[1].valeurBase = 300;
  ListeObjet[1].id = 1;
  strcpy(ListeObjet[1].nom, "Piece Napoléon");
  ListeObjet[1].idAcheteur = -1;

  ListeObjet[2].valeurBase = 600;
  ListeObjet[2].id = 2;
  strcpy(ListeObjet[2].nom, "Statue antique");
  ListeObjet[2].idAcheteur = -1;

  ListeObjet[3].valeurBase = 30000;
  ListeObjet[3].id = 3;
  strcpy(ListeObjet[3].nom, "Voiture James Bond");
  ListeObjet[3].idAcheteur = -1;

  ListeObjet[4].valeurBase = 200000;
  ListeObjet[4].id = 4;
  strcpy(ListeObjet[4].nom, "Appartement 5m² à Paris");
  ListeObjet[4].idAcheteur = -1;

  ListeObjet[5].valeurBase = 200;
  ListeObjet[5].id = 5;
  strcpy(ListeObjet[5].nom, "L'eau de mon bain");
  ListeObjet[5].idAcheteur = -1;
}
int chose_name = 0;
int value_change = 0;
int idObjet = 0;
int Vendu = 0;
int AttenteON = 0;
char sautLIGNE[5] = "\t";
int start = FAUX;
int nb_joueur = 0;
int nb_joueur_avec_nom = 0;
int id = 0;
int maxValue = 0;
Joueur listeJoueurs[NB_WORKERS];
void creerCohorteWorkers(void);
int chercherWorkerLibre(void);
void *threadWorker(void *arg);
void sessionClient(int canal);
int ecrireJournal(char *ligne);
void remiseAZeroJournal(void);

// appelent les fonctions systeme correspondantes + exit si echec
void init_semaphore(sem_t *sem, int valeur);
void wait_semaphore(sem_t *sem);
void post_semaphore(sem_t *sem);
void lock_mutex(pthread_mutex_t *mutex);
void unlock_mutex(pthread_mutex_t *mutex);

// acces au canal d'un worker : le thread principal et un worker peuvent
// acceder simultanement au canal du worker (lors de la recherche d'un worker
// libre pour l'un, lors de la remise du canal a -1 pour l'autre)
// => utilisation d'un mutex par worker; on l'ajoute dans la structure DataSpec

// acces au descripteur du journal : un worker qui ecrit dans le journal et un
// autre qui remet le journal a zero peuvent acceder simultanement au
// descripteur du journal
// => utilisation d'un mutex

int fdJournal;
pthread_mutex_t mutexJournal = PTHREAD_MUTEX_INITIALIZER;
DataSpec dataWorkers[NB_WORKERS];
sem_t semWorkersLibres;

int chercherID(int canal)
{
  for (int i = 0; i < NB_WORKERS; i++)
  {
    if (listeJoueurs[i].canal == canal)
    {
      return listeJoueurs[i].id;
    }
  }
  return -1;
}
void ecrireTousJoueurs(char *buffer, int except_id)
{
  for (int i = 0; i < NB_WORKERS; i++)
  {
    if (listeJoueurs[i].id != -1 && listeJoueurs[i].id != except_id)
    {
      ecrireLigne(listeJoueurs[i].canal, buffer);
    }
  }
}

void startAuction()
{
  Vendu = 0;
  maxValue = ListeObjet[idObjet].valeurBase;
  ecrireTousJoueurs(sautLIGNE, -1);
  char ligneEcrire[LIGNE_MAX];
  snprintf(ligneEcrire, sizeof(ligneEcrire), "Objet mit aux enchères : %s à %d euros", ListeObjet[idObjet].nom, ListeObjet[idObjet].valeurBase);
  ecrireTousJoueurs(ligneEcrire, -1);
  maxValue = ListeObjet[idObjet].valeurBase;
  strcpy(ligneEcrire, "Combien d'euros voulez-vous mettre ?");
  ecrireTousJoueurs(ligneEcrire, -1);
}
void *waitEndAuction(void *arg)
{
  char ligneEcrire[LIGNE_MAX];
  while (AttenteON)
  {
    for (int i = 0; i < 5; i++)
    {
      if (!value_change)
      {
        sleep(1);
        if (!value_change)
        {
          if (i > 1)
          {
            snprintf(ligneEcrire, sizeof(ligneEcrire), "Adjugé dans %d", 5 - i);
            ecrireTousJoueurs(ligneEcrire, -1);
          }
        }
      }
    }
    sleep(1);
    if (!value_change)
    {
      snprintf(ligneEcrire, sizeof(ligneEcrire), "Adjugé vendu à %s", listeJoueurs[ListeObjet[idObjet].idAcheteur].nom);
      ecrireTousJoueurs(ligneEcrire, -1);
      Vendu = 1;
      AttenteON = 0;
      snprintf(ligneEcrire, sizeof(ligneEcrire), "Objet %d : %s vendu à %s pour %d euros", ListeObjet[idObjet].id + 1, ListeObjet[idObjet].nom, listeJoueurs[ListeObjet[idObjet].idAcheteur].nom, maxValue);
      ecrireJournal(ligneEcrire);
      idObjet++;
      if (idObjet == NB_OBJET)
      {
        ecrireTousJoueurs(sautLIGNE, -1);
        strcpy(ligneEcrire, "L'enchère est terminé, nous espèrons que vous avez pu vous offrir votre bonheur !");
        ecrireTousJoueurs(ligneEcrire, -1);
        ecrireTousJoueurs(sautLIGNE, -1);
        strcpy(ligneEcrire, "Merci d'avoir participer ! Vous pouvez accèder aux noms des gagnants depuis le journal !");
        ecrireTousJoueurs(ligneEcrire, -1);
      }
      else
      {
        sleep(3);
        startAuction();
      }
    }
    else
    {
      value_change = 0;
    }
  }
  pthread_exit(NULL);
}
int main(int argc, char *argv[])
{
  remiseAZeroJournal();
  init_obj();
  short port;
  int ecoute, canal, ret;
  struct sockaddr_in adrEcoute, adrClient;
  unsigned int lgAdrClient;
  int numWorkerLibre;
  for (int i = 0; i < NB_WORKERS; i++)
    listeJoueurs[i].id = -1;
  if (argc != 2)
    erreur("usage: %s port\n", argv[0]);

  fdJournal = open(NOM_JOURNAL, O_CREAT | O_WRONLY | O_APPEND, 0600);
  if (fdJournal == -1)
    erreur_IO("ouverture journal");

  port = (short)atoi(argv[1]);

  creerCohorteWorkers();
  init_semaphore(&semWorkersLibres, NB_WORKERS);

  printf("%s: creating a socket\n", CMD);
  ecoute = socket(AF_INET, SOCK_STREAM, 0);
  if (ecoute < 0)
    erreur_IO("socket");

  adrEcoute.sin_family = AF_INET;
  adrEcoute.sin_addr.s_addr = INADDR_ANY;
  adrEcoute.sin_port = htons(port);
  printf("%s: binding to INADDR_ANY address on port %d\n", CMD, port);
  ret = bind(ecoute, (struct sockaddr *)&adrEcoute, sizeof(adrEcoute));
  if (ret < 0)
    erreur_IO("bind");

  printf("%s: listening to socket\n", CMD);
  ret = listen(ecoute, 5);
  if (ret < 0)
    erreur_IO("listen");
  // pthread_t idThread;
  while (VRAI)
  {
    printf("%s: accepting a connection\n", CMD);
    lgAdrClient = sizeof(adrClient);
    canal = accept(ecoute, (struct sockaddr *)&adrClient, &lgAdrClient);
    if (canal < 0)
      erreur_IO("accept");

    printf("%s: adr %s, port %hu\n", CMD,
           stringIP(ntohl(adrClient.sin_addr.s_addr)),
           ntohs(adrClient.sin_port));

    sem_wait(&semWorkersLibres);
    numWorkerLibre = chercherWorkerLibre();
    dataWorkers[numWorkerLibre].canal = canal;
    post_semaphore(&dataWorkers[numWorkerLibre].sem);
    char ligne[LIGNE_MAX];

    strcpy(ligne, "Bienvenue aux enchères !\n");
    ecrireLigne(canal, sautLIGNE);
    ecrireLigne(canal, ligne);
    if (!start)
    {
      strcpy(ligne, "Ecris START pour lancer l'enchère !\n");
      ecrireLigne(canal, ligne);
    }
    listeJoueurs[id].canal = canal;
    listeJoueurs[id].id = id;
    listeJoueurs[id].nom_attribue = 0;
    nb_joueur++;
    strcpy(ligne, "Un nouveau joueur vient de se connecter\n");
    ecrireTousJoueurs(ligne, id);
    snprintf(ligne, sizeof(ligne), "Vous êtes : %d dans la salle", nb_joueur);
    ecrireTousJoueurs(ligne, -1);
    id++;
  }

  if (close(ecoute) == -1)
    erreur_IO("fermeture socket ecoute");

  if (close(fdJournal) == -1)
    erreur_IO("fermeture journal");

  exit(EXIT_SUCCESS);
}

void creerCohorteWorkers(void)
{
  int i;
  int ret;

  for (i = 0; i < NB_WORKERS; i++)
  {
    dataWorkers[i].canal = -1;
    dataWorkers[i].tid = i;
    init_semaphore(&dataWorkers[i].sem, 0);

    ret = pthread_mutex_init(&dataWorkers[i].mutex, NULL);
    if (ret != 0)
      erreur_IO("init mutex");

    ret = pthread_create(&dataWorkers[i].id, NULL, threadWorker,
                         &dataWorkers[i]);
    if (ret != 0)
      erreur_IO("creation worker");
  }
}

// retourne le numero du worker libre trouve ou -1 si pas de worker libre
int chercherWorkerLibre(void)
{
  int i;
  int canal;

  for (i = 0; i < NB_WORKERS; i++)
  {
    lock_mutex(&dataWorkers[i].mutex);
    canal = dataWorkers[i].canal;
    unlock_mutex(&dataWorkers[i].mutex);

    if (canal == -1)
      return i;
  }

  return -1;
}

void *threadWorker(void *arg)
{
  DataSpec *dataSpec = (DataSpec *)arg;

  while (VRAI)
  {
    wait_semaphore(&dataSpec->sem);
    printf("worker %d: reveil\n", dataSpec->tid);

    sessionClient(dataSpec->canal);
    nb_joueur--;
    char ligneEcrire[LIGNE_MAX];
    int id_bind_to_canal = chercherID(dataSpec->canal);
    snprintf(ligneEcrire, sizeof(ligneEcrire), "%s vient de se déconnecter\n", listeJoueurs[id_bind_to_canal].nom);
    ecrireTousJoueurs(ligneEcrire, -1);
    snprintf(ligneEcrire, sizeof(ligneEcrire), "Vous êtes : %d dans la salle", nb_joueur);
    ecrireTousJoueurs(ligneEcrire, -1);
    printf("worker %d: sommeil\n", dataSpec->tid);
    lock_mutex(&dataSpec->mutex);
    dataSpec->canal = -1;
    unlock_mutex(&dataSpec->mutex);

    post_semaphore(&semWorkersLibres);
  }

  pthread_exit(NULL);
}

// session d'echanges avec un client
// fermeture du canal a la fin de la session
void sessionClient(int canal)
{
  int fin = FAUX;
  char ligne[LIGNE_MAX];
  char ligneEcrire[LIGNE_MAX];
  int lgLue;
  // int lgEcr;

  while (!fin)
  {
    if (start && chose_name)
    {
      if (nb_joueur_avec_nom == nb_joueur)
      {
        printf("%d,%d\n", nb_joueur_avec_nom, nb_joueur);
        chose_name = 0;
        startAuction();
      }
      else
        chose_name = 1;
    }
    lgLue = lireLigne(canal, ligne);
    if (lgLue == -1)
      erreur_IO("lecture ligne");

    if (lgLue == 0)
    { // connexion fermee, donc arret brutal du client
      printf("serveur: arret du client\n");
      fin = VRAI;
    }
    else if (strcmp(ligne, "START") == 0)
    {
      start = VRAI;
      if (nb_joueur >= 2)
      {
        ecrireTousJoueurs(sautLIGNE, -1);
        strcpy(ligneEcrire, "Les enchères commencent !");
        ecrireTousJoueurs(ligneEcrire, -1);
        strcpy(ligneEcrire, "Choisissez votre nom :");
        ecrireTousJoueurs(ligneEcrire, -1);
        chose_name = 1;
      }
      else
      {
        strcpy(ligneEcrire, "Pas assez de joueurs dans la salle !");
        ecrireTousJoueurs(ligneEcrire, -1);
      }
    }
    else if (strcmp(ligne, "fin") == 0)
    {
      printf("serveur: fin demandee\n");
      fin = VRAI;
    }
    /*else if (strcmp(ligne, "init") == 0)
    {
      printf("serveur: remise a zero du journal\n");
      remiseAZeroJournal();
    }*/
    else
    {
      if (chose_name)
      {
        int id_bind_to_canal = chercherID(canal);
        if (listeJoueurs[id_bind_to_canal].nom_attribue == 0)
        {
          strcpy(listeJoueurs[id_bind_to_canal].nom, ligne);
          listeJoueurs[id_bind_to_canal].nom_attribue = 1;
          snprintf(ligneEcrire, sizeof(ligneEcrire), "Ton nouveau nom est %s", listeJoueurs[id_bind_to_canal].nom);
          nb_joueur_avec_nom++;
        }
        else
        {
          strcpy(listeJoueurs[id_bind_to_canal].nom, ligne);
          snprintf(ligneEcrire, sizeof(ligneEcrire), "Ton nouveau nom est %s", listeJoueurs[id_bind_to_canal].nom);
        }
        ecrireLigne(canal, ligneEcrire);
      }
      else if (start)
      {
        printf("%d > %s\n", canal, ligne);
        int value = atoi(ligne);
        if (value > maxValue && !Vendu)
        {
          int id_bind_to_canal = chercherID(canal);
          if (id_bind_to_canal != ListeObjet[idObjet].idAcheteur)
          {
            value_change = 1;
            maxValue = value;
            snprintf(ligneEcrire, sizeof(ligneEcrire), "%s surenchérit à %d euros", listeJoueurs[id_bind_to_canal].nom, maxValue);
            ecrireTousJoueurs(ligneEcrire, id_bind_to_canal);
            snprintf(ligneEcrire, sizeof(ligneEcrire), "Ton enchère à %d est acceptée", maxValue);
            ecrireLigne(canal, ligneEcrire);
            ListeObjet[idObjet].idAcheteur = id_bind_to_canal;
            if (!AttenteON)
            {
              AttenteON = 1;
              pthread_t idThread;
              pthread_create(&idThread, NULL, waitEndAuction, &value);
            }
          }
          else
          {
            snprintf(ligneEcrire, sizeof(ligneEcrire), "Tu es déjà la proposition la plus élevé à %d euros", maxValue);
            ecrireLigne(canal, ligneEcrire);
          }
        }
        else
        {
          snprintf(ligneEcrire, sizeof(ligneEcrire), "Le prix est trop bas, %d <= %d", value, maxValue);
          ecrireLigne(canal, ligneEcrire);
        }
      }

      /*lgEcr = ecrireJournal(ligne);
      if (lgEcr == -1)
        erreur_IO("ecriture journal");
      printf("serveur: ligne de %d octets ecrite dans le journal\n", lgEcr);*/
    }
  }

  if (close(canal) == -1)
    erreur_IO("fermeture canal");
}

int ecrireJournal(char *ligne)
{
  int lg;

  lock_mutex(&mutexJournal);
  lg = ecrireLigne(fdJournal, ligne);
  unlock_mutex(&mutexJournal);

  return lg;
}
void remiseAZeroJournal(void)
{
  // on ferme le fichier et on le rouvre en mode O_TRUNC
  lock_mutex(&mutexJournal);
  if (close(fdJournal) == -1)
    erreur_IO("fermeture jornal pour remise a zero");

  fdJournal = open(NOM_JOURNAL, O_TRUNC | O_WRONLY | O_APPEND, 0600);
  if (fdJournal == -1)
    erreur_IO("ouverture journal pour remise a zero");
  unlock_mutex(&mutexJournal);
}

void init_semaphore(sem_t *sem, int valeur)
{
  int ret;
  ret = sem_init(sem, 0, valeur);
  if (ret != 0)
    erreur_IO("init semaphore");
}

void wait_semaphore(sem_t *sem)
{
  int ret;
  ret = sem_wait(sem);
  if (ret != 0)
    erreur_IO("wait semaphore");
}

void post_semaphore(sem_t *sem)
{
  int ret;
  ret = sem_post(sem);
  if (ret != 0)
    erreur_IO("post semaphore");
}

void lock_mutex(pthread_mutex_t *mutex)
{
  int ret;
  ret = pthread_mutex_lock(mutex);
  if (ret != 0)
    erreur_IO("lock mutex");
}

void unlock_mutex(pthread_mutex_t *mutex)
{
  int ret;
  ret = pthread_mutex_unlock(mutex);
  if (ret != 0)
    erreur_IO("unlock mutex");
}
