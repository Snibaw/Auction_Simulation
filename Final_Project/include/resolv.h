#ifndef RESOLV_INCLUDE_H
#define RESOLV_INCLUDE_H

/* module resolv: resolution DNS */

/*
  la fonction resolv convertit un nom d'hote et un nom de service en une
  adresse IP et un port exprimes sous forme d'une structure struct sockaddr_in
  dont elle retourne un pointeur;
  en cas d'erreur, elle affiche un message et retourne NULL;
  la structure retournee est valide jusqu'au prochain appel a resolv
*/
struct sockaddr_in *resolv(const char *host, const char *service) ;

/*
  la fonction stringIP convertit une adresse IP exprimee sous forme d'un entier
  32 bits dans l'orde de l'hote en une adresse IP exprimee sous forme d'une
  chaine aaa.bbb.ccc.ddd;
  elle retourne NULL en cas d'erreur;
  la chaine retournee est dans une variable statique, donc ecrasee a chaque
  appel
*/
char *stringIP(uint32_t entierIP);

#endif
