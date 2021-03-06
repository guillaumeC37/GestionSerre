/* * * * * * * * * * * * * * * * * * * * * * * * * * */
/*            Gestion Serre intelligente             */
/*   G. Cregut                                       */
/*   DATE Création : 14/11/2016                      */
/*   Date Modification : 04/12/2016                  */
/* (c)2016 Editiel98                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * */

//déclarations includes
#include <Wire.h>  //pour l'EEPROM, la RTC et l'écran.
#include <EEPROM.h> // Pour le stockage des paramètres dans la PROM de l'arduino
#include "structDate.h" //stucture de la date
#include <LiquidCrystal_I2C.h>  //Pour le controle de l'aficheur I2C
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                         */
/*                        Déclarations variables globales                  */
/*                                                                         */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int TempExt, TempInt, MesHygro, MesLum ;  //Retour des mesures de températures
bool FaireMesure;   //Toggle pour déclencher les mesures. En fonction de la période des mesures
bool DebutScript; // indique qu'on démarre la carte
bool BackLightLCD;  //Etat du backlight du LCD
bool EtatVolet, EtatLumiere,EtatVanne, EtatChauffage;
bool InMenu; //Défini si on est entrer dans le menu ou non
int TrigHygro, TrigTemp, TrigLumiere,TrigOuverture;  //valeur a partir du moment ou on arrose, aere ou allume la lumière
int PointeurEEPROM;  //Pointeur de position dans la prom. Pas forcement INT, a voir !
byte TempoTrig,HeureMesure; //Delai en heure entre 2 mesures, Heure de la dernière mesure
int NombreRecord; //Nombre d'enregistrement dans l'EEPROM
unsigned long ReinitBackLight;
long int Maintenant;  //Pour l'extinction de l'écran. Lancé a chaque appui. Scanné dans la boule principale
                                       //Structure pour lire et écrire dans la RTC
DateRTC DateMesure;
MesureEEPROM MesureFaite;  //Génère une structure type mesure
int IdMenu; //position dans le menu
byte EntreeMenu;  //Compteur pour passer dans le menu
byte IdHorsMenu; //Position dans le "hors menu"
int ValeurMenu[5];
                                      //Déclaration des variables pour le protocole
byte IdRecu=0;
bool POK=false;
bool SortiePil;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                         */
/*                       Déclarations constantes globales                  */
/*                                                                         */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  //Adresses Bus
#define Adress_RTC 0     //A changer
#define AdresseLCD 0x27     //A vérfier
#define AdresseEEPROM 2     //A changer
//Entrées analogiques
#define PinTempInt A0  //Broche connectée au capteur intérieur.
#define PinTempExt A1
#define PinHygro A2
#define PinLuminosite A3
#define PortSpeed 9600
//Entrées numériques  //Changer les valeurs
#define BoutonMenu 1
#define BoutonHaut 2
#define BoutonGauche 3
#define BoutonDroit 4
#define BoutonBas 5

//sorties  //Changer les valeurs
#define PinVanne 6
#define PinChauffage 7
#define PinServo 8
#define PinLumiere 9

//Bus

//Declaration des types de mesures
#define TempExtM 1
#define TempIntM 2
#define HygroM 3
#define LuminositeM 4

//Définitions pour les menus
#define MenuTempExt 0
#define MenuTempInt 1
#define MenuMesures 2
#define MenuCdeVanne 3
#define MenuCdeChauff 4
#define MenuCdeLum 5
#define MenuCdeVolet 6
#define LimiteHorsMenu 6  //7 menus de 0 à 6
#define LimiteMenu 5  //7 menus de 0 à 6
#define MTrigOuv 0
#define MTrigHygro 1
#define MTrigLum 2
#define MTrigTemp 3
#define Mtempo 4
#define Quitter 5

//Definition du temps d'éclairage en ms de l'afficheur
#define TempsMilliBackLight 30000   //30 secondes d'affichage de l'écran.

                                //Déclarations diverses 
LiquidCrystal_I2C MonEcran(AdresseLCD, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                         */
/*                          Déclarations des fonctions                     */
/*                                                                         */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

                     /********Fonctions nécessaires pour la RTC********/ 
  //Fonction conversion BCD vers Décimale
byte BCD2Dec(byte bcd)
{
  return((bcd /16*10)+(bcd%16));
}
  //Fonction conversion Décimale vers BCD
byte Dec2BCD(byte dec)
{
  return((dec/10*16)+(dec%10));
}
                     /********Fonction Lecture de la RTC********/ 
void RecupereDateHeure(DateRTC *date)
{
  //Récupère la date de la RTC
  byte drop; //supprime le jour de la semaine
  Wire.beginTransmission(Adress_RTC);  //Initialise la com vers la RTC
  Wire.write(0);  //Demande les infos a partir de l'adresse 0
  Wire.endTransmission(); //Fin d'écriture de la demande RTC
  Wire.requestFrom(Adress_RTC,7);  //Demande 7 octets
  date->secondes=BCD2Dec(Wire.read());
  date->minutes=BCD2Dec(Wire.read());
  date->heures=BCD2Dec(Wire.read()& 0b111111);
  drop=BCD2Dec(Wire.read());
  date->jour=BCD2Dec(Wire.read());
  date->mois=BCD2Dec(Wire.read());
  date->annee=BCD2Dec(Wire.read());
  //Stocke l'heure de mesure
  HeureMesure=date->heures;
}

                     /********Fonction Ecriture de la RTC********/ 
void EcrireRTC(DateRTC *date)
{
  byte Drop;   //on s'en fout
  Drop=1;
  Wire.beginTransmission(Adress_RTC);  //Initialise la com vers la RTC
  Wire.write(0);
  Wire.write(Dec2BCD(date->secondes));
  Wire.write(Dec2BCD(date->minutes));
  Wire.write(Dec2BCD(date->heures));
  Wire.write(Dec2BCD(Drop));
  Wire.write(Dec2BCD(date->jour));
  Wire.write(Dec2BCD(date->mois));
  Wire.write(Dec2BCD(date->annee));
  Wire.write(0);
  Wire.endTransmission(); //Fin d'écriture de la demande RTC
}
                     /********Fonction Conversion 2 Bytes en Int ********/ 
int ConvertByteToInt(byte ValMsb, byte ValLsb)
{
  return (ValMsb<<8)|ValLsb;
}

                     /********Fonction Ecriture d'un octet en mémoire ********/ 
void I2CEEPROMWrite(int Adresse,byte Valeur)
{
  Wire.beginTransmission(AdresseEEPROM);  //adresse de la prom 
  Wire.write((Adresse>>8)&0xFF);
  Wire.write((Adresse>>0)&0xFF);
  Wire.write(Valeur);
  Wire.endTransmission();
  delay(5);                    
}

                     /********Fonction Lecture d'un octet en mémoire ********/ 
byte I2CEEPROMRead(int Adresse)
{
  byte tempo=0xFF;  //Au cas ou on ai rien lu
  Wire.beginTransmission(AdresseEEPROM);  //adresse de la prom
  Wire.write((Adresse>>8)&0xFF);
  Wire.write((Adresse>>0)&0xFF);
  Wire.endTransmission();
  delay(5);
  Wire.requestFrom(AdresseEEPROM,1);
  tempo=Wire.read();
  delay(5);
  return tempo;
}
                     /********Fonction Gestion de l'EEPROM I2C********/ 
//Fonction écriture dans l'EEPROM I2C
int EcrireEEPROM(int debut,MesureEEPROM *MesureAEnregistrer)
{
//Variables
  byte MSBVal,LSBVal;  //On écrit octet par octet.
  byte MesuresAEcrire[8];
  MSBVal=highByte(MesureAEnregistrer->ValMesure);
  LSBVal=lowByte(MesureAEnregistrer->ValMesure);
 /* MSBVal=(MesureAEnregistrer->ValMesure &0xFF00)>>8;
  LSBVal=MesureAEnregistrer->ValMesure&0x00FF;*/
  /*      Schéma adressage
   *  -Adresse 0, soit début : Minutes
   *  -Adresse+1 : Heures
   *  -Adresse+2 : Jour
   *  -Adresse+3 : Mois
   *  -Adresse+4 : Annee
   *  -Adresse+5 : Type Mesure
   *  -Adresse+6 : MSB
   *  -Adresse+7 : LSB
   */
   //On affecte les mesures à un tableau
   MesuresAEcrire[0]=MesureAEnregistrer->HeureEEPROM;
   MesuresAEcrire[1]=MesureAEnregistrer->MinuteEEPROM;
   MesuresAEcrire[2]=MesureAEnregistrer->JourEEPROM;
   MesuresAEcrire[3]=MesureAEnregistrer->MoisEEPROM;
   MesuresAEcrire[4]=MesureAEnregistrer->AnneeEEPROM;
   MesuresAEcrire[5]=MesureAEnregistrer->TypeMesure;
   MesuresAEcrire[6]=MSBVal;
   MesuresAEcrire[7]=LSBVal;
  //Verifions si on déborde pas.... On est à l'adresse d'écriture
  //Si adresse+taille enregistrement >taille EEPROM, adresse=0
  if (debut>65527)  //65535-8 car on a 8 octets dans l'enregistrement
  {
    debut=0;
    NombreRecord=0;
  }
  //Ecrit dans l'eeprom
   for (int i=0;i<8;i++)
   {
      I2CEEPROMWrite(debut+i,MesuresAEcrire[i]);
   }
  //augmente le pointeur de la taille d'une structure
  debut=debut+sizeof(MesureEEPROM);  
  NombreRecord++;
  return debut;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                         */
/*                         Fonctions mesures et actions                    */
/*                                                                         */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */  
int MesureTemp(byte TypeTemp)  //Mesure la température
{
  int AdresseMin,  AdresseMax, ValeurLue;
  //Mesure la température intérieure, extérieure. Compare avec le min et le max, et si valeurdépassée, stocke en EEPROM
  //Stocke les valeurs dans les variables globales, pour les fonctions de traitement
   MesureFaite.TypeMesure=TypeTemp;
  //Lecture de la mesure
  switch(TypeTemp)  //En fonction du type de capteur (interieur ou exterieur)
  {
    case TempExtM :
      MesureFaite.ValMesure=analogRead(PinTempExt);
      //Adresse Min et Max dans l'EEPROM arduino
      AdresseMin=sizeof(int)*2;  //L'adresse Temp Ext Min est la 3ème position après 2 autres int
      AdresseMax=sizeof(int)*3;  //L'adresse Temp Ext Max est la 4ème position après 3 autres int
      TempExt=MesureFaite.ValMesure;
      break;
    case TempIntM : 
      MesureFaite.ValMesure=analogRead(PinTempInt);
      TempInt=MesureFaite.ValMesure;
      //Adresse Min et Max dans l'EEPROM arduino
      AdresseMin=0;  //La veleur de temp Int min est à l'adresse 0
      AdresseMax=sizeof(int); //La valeur temp Int Max est la suivante, soit 0+taille de int
      break; 
  }  
  //Vérifie sur mesureTemp.ValMesure<min ou >max
   EEPROM.get(AdresseMin,ValeurLue);  //On récupère la temp Min
   if (MesureFaite.ValMesure<ValeurLue)
   {
      //On stocke la valeur et on passe
      EEPROM.put(AdresseMin,MesureFaite.ValMesure); 
   }
   else  //Si la mesure est au dessus de la min elle peut etre max.
   {
       EEPROM.get(AdresseMax,ValeurLue);  //On récupère la temp Max
       if (MesureFaite.ValMesure>ValeurLue)
       {
          //On stocke la valeur max
           EEPROM.put(AdresseMax,MesureFaite.ValMesure);
       }
   }
  //Enregistrement de la mesure dans la PROM
  PointeurEEPROM=EcrireEEPROM(PointeurEEPROM, &MesureFaite);
}

                     /********Fonction Gestion de Chauffage / Aération********/  
void GestionAreoChauffage(int Interieur, int Exterieur)
{
  //En fonction de la température, algorithme qui défini l'ouverture de la serre, la mise en route du ventilo ou le chauffage
  //TrigTemp,TrigOuverture
 // Gere_Chauffage();
 // Gere_Volet(); 
}

                     /********Fonction Mesure Hyrgro********/
int MesureHygro()
{
  //Mesure l'hygrométrie et la stocke en mémoire
   MesureFaite.TypeMesure=HygroM;
   //Lecture de la mesure
   MesureFaite.ValMesure=analogRead(PinHygro); 
   MesHygro=MesureFaite.ValMesure;
   //Enregistrement de la mesure dans la PROM
   PointeurEEPROM=EcrireEEPROM(PointeurEEPROM, &MesureFaite);   
   return MesureFaite.ValMesure; 
}

                     /********Fonction Gestion de Vanne********/
void GestionVanne(int Hygro)
{
  //Agit sur l'electrovanne en fonction de l'hygrométrie
  int HygroBascule;
  //On créé un trigger de schmitt pour ne pas faire que arroser arret arrosage
  HygroBascule=(TrigHygro/100);
  HygroBascule=(HygroBascule*10)+TrigHygro;   //10% inférieur au seuil
  if (Hygro<HygroBascule)
  {
    //On ouvre la vanne
    Gere_Vanne(true);
  }
  else  //si on est pas en bas, on peut etre en haut...
  {
    HygroBascule=(TrigHygro/100);
    HygroBascule=(HygroBascule*20)+TrigHygro;  //20 % supérieur au seuil
    if (Hygro>HygroBascule)
    {
    //On ferme la vanne
      Gere_Vanne(false);
    }
  }  
}

                     /********Fonction Mesure Luminosité********/
int MesureLumiere()
{
  //Mesure la luminosité. Idem, stocke et retourne la valeur mesurée
  MesureFaite.TypeMesure=LuminositeM;
  //Lecture de la mesure
  MesureFaite.ValMesure=analogRead(PinLuminosite);  
  MesLum=MesureFaite.ValMesure;
  //Enregistrement de la mesure dans la PROM
  PointeurEEPROM=EcrireEEPROM(PointeurEEPROM, &MesureFaite);   
  return MesureFaite.ValMesure;  
}

                     /********Fonction Gestion Lumiere********/
void GestionLumiere(int LumiereMesuree)
{
  //En fonction de la lumière, allume ou non la lumière
  int LumiereBascule;
  //On créé un trigger de schmitt pour ne pas faire que arroser arret arrosage
  LumiereBascule=(TrigLumiere/100);
  LumiereBascule=(LumiereBascule*10)+TrigLumiere;   //10% inférieur au seuil
  if (LumiereMesuree<LumiereBascule)
  {
    //On allume la lumière
    Gere_Lumiere(true);
  }
  else  //si on est pas en bas, on peut etre en haut...
  {
    LumiereBascule=(TrigLumiere/100);
    LumiereBascule=(LumiereBascule*20)+TrigLumiere;  //20 % supérieur au seuil
    if (LumiereMesuree>LumiereBascule)
    {
    //On éteint la lumière
     Gere_Lumiere(false);
    }
  }    
}

                     /********Fonction Action chauffage ********/
void Gere_Chauffage(bool EtatNouveau)
{
  if(EtatNouveau)
  {
    
  }
  else
  {
    
  }
}
                     /********Fonction Action Volet ********/      
void Gere_Volet(bool EtatNouveau)
{
  if(EtatNouveau)
  {
    
  }
  else
  {
    
  }  
}                     
                     /********Fonction Action Lumière ********/
void Gere_Lumiere(bool EtatNouveau)
{
  if(EtatNouveau)
  {
    //On allume la lumière
    digitalWrite(PinVanne,HIGH);
    EtatLumiere=true;
  }
  else
  {
    //On allume la lumière
    digitalWrite(PinVanne,LOW);
    EtatLumiere=false;
  }  
}
                     /********Fonction Action vanne ********/
void Gere_Vanne(bool EtatNouveau)
{
  if(EtatNouveau)
  {
    //On ouvre la vanne
    digitalWrite(PinVanne,HIGH);
    EtatVanne=true;
  }
  else
  {
    //On ouvre la vanne
    digitalWrite(PinVanne,LOW);
    EtatVanne=false;
  }  
}

                     /********Fonction Sauvegarde en EEPROM ********/
void EcritEEPROM_Config()
{
  EEPROM.write(sizeof(int)*4,TrigOuverture);
  EEPROM.write(sizeof(int)*6,TrigHygro);
  EEPROM.write(sizeof(int)*8,TrigTemp);
  EEPROM.write(sizeof(int)*7,TrigLumiere);
  //Lecture de l'intervalle de mesure  
  EEPROM.write(sizeof(int)*9,TempoTrig);  
}

                     /********Fonction Affichage   ********/
 
 void AfficheHorsMenu(String MessageAAfficherL1,String MessageAAfficherL2)
{
  if (MessageAAfficherL1=="")
  {
    //on a cliquer sur menu/OK ou on change les valeurs, on change donc la ligne 2 uniquement
    MonEcran.setCursor(0,1); //On passe à la seconde ligne
    MonEcran.print(MessageAAfficherL2);
  }
  else
  {
    MonEcran.clear(); //On efface l'écran
    MonEcran.setCursor(0,0); //On met le curseur en début de la 1ere ligne
    MonEcran.print(MessageAAfficherL1);
    MonEcran.setCursor(0,1); //On passe à la seconde ligne
    MonEcran.print(MessageAAfficherL2);
  }
}        

 void AfficheMenu(int IdDuMenu)
{
  int ValAff;
  switch(IdDuMenu)
  {
   case MTrigOuv :
      //lecture de l'EEPROM pour trig ouverture et affichage
      ValAff=ValeurMenu[MTrigOuv];     
      AfficheHorsMenu("Trig Ouv",String(ValAff));
    break;
    case MTrigHygro :
      ValAff=ValeurMenu[MTrigHygro];   
      AfficheHorsMenu("Trig Hygro",String(ValAff));
    break;
    case MTrigLum :
      ValAff=ValeurMenu[MTrigLum];    
      AfficheHorsMenu("Trig Lum",String(ValAff));
    break;
    case MTrigTemp :
      ValAff=ValeurMenu[MTrigTemp];      
      AfficheHorsMenu("Trig Temp",String(ValAff));
    break;
    case Mtempo :
      ValAff=ValeurMenu[Mtempo];    
      AfficheHorsMenu("Temp",String(ValAff));
    break;
    case Quitter :
      AfficheHorsMenu("Quitter ?","");
    break;
  }
}           
                     /********Fonction Menu   ********/
void bouton_appuye()
{
  //Gestion de l'affichage hors menu
  String AfficheMessage;
  boolean EtatBoutonH=digitalRead(BoutonHaut);
  boolean EtatBoutonB=digitalRead(BoutonBas);
  boolean EtatBoutonG=digitalRead(BoutonGauche);
  boolean EtatBoutonD=digitalRead(BoutonDroit);
  boolean EtatBoutonM=digitalRead(BoutonMenu);
  String EtatActionneur,EtatActionneurM;
  Maintenant=millis();   //On relance le décompte pour l'extinction
  if(!BackLightLCD)
  {
    MonEcran.backlight();
    ReinitBackLight=millis();  //On mémorise le départ de l'éclairage
  }
  if (!EtatBoutonH || !EtatBoutonB || !EtatBoutonG || !EtatBoutonD || !EtatBoutonM)
  {
    //on a appuyer sur un bouton
    if (!InMenu)
    {
      //On est pas dans le menu
      //a t'on appuyé sur menu ?
      if (!EtatBoutonM) //a modifier..
      {
        //on a appuyer sur menu/OK
        //en fonction du menu, on effectue les modifications
        switch (IdHorsMenu)
        {
           case MenuCdeVanne :
             if(EtatVanne)
             {
                EtatActionneurM="OFF    ";
             }
             else
               EtatActionneurM="ON     ";
             EtatVanne=!EtatVanne;
             Gere_Vanne(EtatVanne);
             AfficheHorsMenu("",EtatActionneurM);
           break;
           case MenuCdeChauff : 
           if(EtatChauffage)
             {
                EtatActionneurM="OFF    ";
             }
             else
               EtatActionneurM="ON     ";
             EtatChauffage=!EtatChauffage;
             Gere_Chauffage(EtatChauffage);
             AfficheHorsMenu("",EtatActionneurM);
           break;  
           case MenuCdeLum : 
             if(EtatLumiere)
             {
                EtatActionneurM="OFF    ";
             }
             else
               EtatActionneurM="ON     ";
             EtatLumiere=!EtatLumiere;
             Gere_Lumiere(EtatLumiere);
             AfficheHorsMenu("",EtatActionneurM);
           break;    
           case MenuCdeVolet : 
             if(EtatVolet)
             {
                EtatActionneurM="OFF    ";
             }
             else
               EtatActionneurM="ON     ";
             EtatVolet=!EtatVolet;
             Gere_Volet(EtatVolet);          
             AfficheHorsMenu("",EtatActionneurM);
           break; 
           default :
             EntreeMenu++;
           break;               
        }
        if (EntreeMenu==2)
        {
          AfficheMenu(MTrigOuv);
          InMenu=true;
          IdMenu=0;
        }
        exit;
      }
      else
      {
        if (!EtatBoutonH || !EtatBoutonG)
        {
           //on décrimente le hors menu
           EntreeMenu=0;
           if (IdHorsMenu==0) //On evite de sortir du tableau, donc on reboucle
           {
              IdHorsMenu=LimiteHorsMenu; 
           }
           else
             IdHorsMenu--;
        }
        else if (!EtatBoutonB || !EtatBoutonD)
        {
          //on incrimente le hors menu
          if (IdHorsMenu==LimiteHorsMenu)  //On evite de sortir du tableau
          {
             IdHorsMenu=0;
          }
          else
          {
             IdHorsMenu++;
          }
        }  
         //Maintenant, on regarde quel menu afficher 
        switch(IdHorsMenu)
        {
          case MenuTempExt :
            //Récupérer les valeurs Temp Ext et les min/max
            AfficheMessage="EXT ";
            AfficheMessage+="10.5";
            AfficheMessage+="C-";
            AfficheMessage+= "25.5";
            AfficheMessage+= "C";
            AfficheHorsMenu(AfficheMessage,"25.5C");
          break;
          case MenuTempInt :
            //Récupérer les valeurs Temp Int et min /max                     
            AfficheMessage="INT ";
            AfficheMessage+="10.5";
            AfficheMessage+="C-";
            AfficheMessage+= "25.5";
            AfficheMessage+= "C";
            AfficheHorsMenu(AfficheMessage,"25.5C");
          break;
          case MenuMesures :
            //Récuperer les valeurs hygro et Lum
            AfficheMessage="Hygro ";
            AfficheMessage+="25";
            AfficheMessage+="%-LUM ";
            AfficheMessage+="35";
            AfficheHorsMenu(AfficheMessage,"");
          break;
          case MenuCdeVanne :
            //Récupérer l'etat de la Vanne
            // Serial.print("Etat Vanne");
            EtatActionneur="OFF";
            if (EtatVanne)
            {
              EtatActionneur=true;
              EtatActionneur="ON    ";
            }
            AfficheHorsMenu("Etat Vanne",EtatActionneur);
          break;
          case MenuCdeChauff :
            //Récuperer l'état du chauffage
            EtatActionneur="OFF";
            if (EtatChauffage)
            {
              EtatActionneur=true;
              EtatActionneur="ON    ";
            }
            AfficheHorsMenu("Etat Chauffage",EtatActionneur);
          break;
          case MenuCdeLum :
            //Récupérer l'état de la lumière
            EtatActionneur="OFF";
            if (EtatLumiere)
            {
              EtatActionneur=true;
              EtatActionneur="ON    ";
            }
            AfficheHorsMenu("Etat Lumiere",EtatActionneur);
          break;
          case MenuCdeVolet :
            //Récupérer l'état du volet
            EtatActionneur="OFF";
            if (EtatVolet)
            {
              EtatActionneur=true;
              EtatActionneur="ON    ";
            }
            AfficheHorsMenu("Etat Volet",EtatActionneur);
          break;
        }
      }
      ////////////////////////////Fin de ce qui ne doit pas être fait si menu appuyé
    }
    else
    {
      //On est dans le menu
     //a t'on appuyé sur menu ?
      if (!EtatBoutonM) //a modifier..
      {
        //on a appuyer sur menu/OK
        //en fonction du menu, on effectue les modifications
        if (IdMenu==Quitter)
        {
           //sauvegarde
           TrigOuverture=ValeurMenu[MTrigOuv];
           TrigHygro=ValeurMenu[MTrigHygro];
           TrigLumiere=ValeurMenu[MTrigLum];
           TrigTemp=ValeurMenu[MTrigTemp];
           TempoTrig=ValeurMenu[Mtempo];
           EcritEEPROM_Config();
           //on quitte le menu
           InMenu=false;
           EntreeMenu=0;
           IdHorsMenu=MenuTempExt;
           //affiche hors menu 
           AfficheHorsMenu(" "," ");
        }
      }
      else
      {
        if (!EtatBoutonH)
        {
           //on décrimente le menu
           if (IdMenu==0) //On evite de sortir du tableau, donc on reboucle
           {
              IdMenu=LimiteMenu; 
           }
           else
             IdMenu--;
           //On regarde le menu a afficher
           AfficheMenu(IdMenu);  
        }
        else if (!EtatBoutonB)
        {
          //on incrimente le hors menu
          if (IdMenu==LimiteMenu)  //On evite de sortir du tableau
          {
             IdMenu=0;
          }
          else
          {
             IdMenu++;
          }
          //On regarde le menu à afficher
          AfficheMenu(IdMenu); 
        }  
        else if (!EtatBoutonG) //si on a appuyer sur gauche ou droite
        {
           ValeurMenu[IdMenu]--;
           if (ValeurMenu[IdMenu]<0)
           {
              ValeurMenu[IdMenu]=0;
           }
           AfficheHorsMenu("",String(ValeurMenu[IdMenu]));
        }
        else if (!EtatBoutonD) //si on a appuyer sur gauche ou droite
        {
           ValeurMenu[IdMenu]++;
           if (ValeurMenu[IdMenu]>1024)
           {
              ValeurMenu[IdMenu]=1024;
           }
           AfficheHorsMenu("",String(ValeurMenu[IdMenu]));
        }
      }//Si  autre bouton que menu
    }
  }
  delay(300); //anti rebond  
}
void ReinitialiseMesure()
{
  FaireMesure=false;
  //on créé le prochain moment de faire une mesure
  HeureMesure=DateMesure.heures+TempoTrig;
}

                     /********Fonctions Protocole Série   ********/
void LitSerial()
{
  String MaChaine="";
  if(Serial.available())
  {
    while(Serial.available())
    {
      char c=Serial.read();
       MaChaine+=String(c);
       delay(10);
    }
    MaChaine.trim();
    analyseprotocole(MaChaine);
    MaChaine="";
  }
}


                     /********Fonctions Protocole Série  Analyse ********/
void analyseprotocole(String recu)
{
  bool GetRecord=false;
  unsigned int i;
  String Reponse;
  GetRecord=recu.startsWith("GR",0);
   //On est dans le protocole
  if (POK)
  {
    //On est dans le protocole, on traite les receptions
    IdRecu=0;
    if(recu=="PNOK")
    {
      IdRecu=7;
    }
    if(recu=="G1")
    {
      IdRecu=1;
    }
    if(recu=="G2")
    {
      IdRecu=2;
    }
    if(recu=="G3")
    {
      IdRecu=3;
    }
    if(recu=="G4")
    {
      IdRecu=4;
    }
    if(recu=="M")
    {
      IdRecu=5;
    }
    if(recu=="ER")
    {
      IdRecu=9;
    }
    if (GetRecord)
    {
      IdRecu=6;
    }
    SortiePil=recu.startsWith("S");
    if (SortiePil)
    {
      IdRecu=8;
    }
    
    switch(IdRecu)
    {
      case 0:  //On a pas recu une commande valide
        Serial.println("PNOK"); //
        POK=false;
        break;  
      case 1:  //On a recu G1
       //Lecture de la température intérieure
        Serial.println("");  //Envoie la valeur brut de lecture en int
        break;  
      case 2:  //On a recu G2
        //Lecture de la température extérieure
        Serial.println(""); //Envoie la valeur brut de lecture en int
        break; 
      case 3:  //On a recu G3
        //Lecture Hygro
        Serial.println(""); //Envoie la valeur brut de lecture en int
        break; 
      case 4:  //On a recu G4
        //Lecture Luminosité
        Serial.println(""); //Envoie la valeur brut de lecture en int
        break; 
      case 5:  //On a recu M
        //On envoie le nombre d'enregistrement présent dans l'EEPROM
        Serial.println("20");
        break;  
      case 6:  //On a recu GetRecord

        i=recu.length();  //taille du message recu
        if (i>8) //GR_65535 c'est 8 caractères
        {
           Serial.println("PNOK");
           POK=false;
        }
        recu=recu.substring(3,i);  //on supprime le "GR_"
        i=recu.toInt();
        if(i<=0)
        {
           Serial.println("PNOK");
           POK=false; 
        }
        else
        {
      //On retourne le numéro de l'enregistrement et les infos
          Reponse=i;
          Reponse+="-31-12-2016-18-41-1-1024";
          Serial.println(Reponse);
        }
        break;
      case 7:  //On a recu PNOK
        POK=false;
     //   Serial.println("Recu PNOK");         //Debug
        break; 
      case 8:  //On a recu une commande de sortie
        char Sortie, valeur;
        int ValMesRetour;
        i=recu.length();  //taille du message recu
        if (i==4)
        {
          Sortie=recu[1];
      /*  Serial.print("Recu S+");         //Debug
        Serial.println(Sortie);         //Debug*/
        
          valeur=recu[3]; 
        }
        else
        {
          Sortie='X';  
        }
        //////////////////////////////////////
        switch(Sortie)
        {
           case 'C' : //C'est le chauffage
             if (valeur=='0')
             {
                //On ferme le chauffage
                digitalWrite(13,LOW);
                //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
             }
             else if (valeur=='1')
             {
                //On ouvre le chauffage
                digitalWrite(13,HIGH);
                //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
             }
            else if (valeur='?')
            {
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }             
             else
             {
                Serial.println("PNOK SC");
                POK=false;
             } 
             break;
           case 'V' :  //C'est la vanne
            if (valeur=='0')
            {
              //On ferme la vanne
                digitalWrite(13,LOW);
              //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }
            else if (valeur=='1')
            {
              //On ouvre la vanne
                digitalWrite(13,HIGH);
              //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }
            else if (valeur='?')
            {
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }            
            else
            {
              Serial.println("PNOK SV");
              POK=false;
            }
            break;
          case 'T' :  //C'est le volet
                  if (valeur=='0')
            {
              //On ferme la volet
                digitalWrite(13,LOW);
             //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }
            else if (valeur=='1')
            {
              //On ouvre la volet
                digitalWrite(13,HIGH);
              //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }
            else if (valeur='?')
            {
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }
            else
            {
              Serial.println("PNOK ST");
              POK=false;
            }
            break;
          case 'L':  // C'est la lumière
            if (valeur=='0')
            {
              //On ferme la lumière
                digitalWrite(13,LOW);
              //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
            //    Serial.println("Mise a 0");      ///debug
                Serial.println(ValMesRetour);
            }
            else if (valeur=='1')
            {
              //On ouvre la lumière
                digitalWrite(13,HIGH);
             //On lit la valeur effective
                delay(100);
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
              //  Serial.println("Mise a 1");      ///debug
                Serial.println(ValMesRetour);
            }
            else if (valeur='?')
            {
                ValMesRetour=digitalRead(13);  //Changer pour la bonne broche
                Serial.println(ValMesRetour);
            }
            else
            {
              Serial.println("PNOK SL");
              POK=false;
            }
            break;
          default :
            //Erreur de protocole, sortie
            Serial.println("PNOK S");
            POK=false;
            break;  
        }      
        /////////////////////////////////////
        break; 
        case 9:   //On efface la mémoire
          PointeurEEPROM=0;  //Réinitialisation du pointeur, revient à faire un effacement.
          Serial.println("OK");
          break;          
    }
  }  //Fin de on est dans le protocole
    //On initialise le protocole
  if (!POK)
  {  
    if (recu=="?")
    {
      Serial.println("GestionSerre");
    }
    else
    {
      if (recu=="V")
      {
        Serial.println("1.0");
      }
      else
      {
        if (recu=="POK")
        {
          POK=true;  //Le logiciel nous informe qu'il accepte le protocole
        }
        else
        {
          POK=false;
        }
      }  
    }
  }  
}
                     

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                Initialisation                                       */
/*  On Va cherches les infos diverses dans l'eeprom                                    */
/*  On initialise les différentes broches des ports                                    */
/*  On initialise la mémoire EEPROM extérieure                                         */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void setup() {
  //Initialisation système
  InMenu=false;
  //Initialise les bus
  
  //Initialise les ports
  pinMode(BoutonMenu, INPUT);
  pinMode(BoutonGauche, INPUT);
  pinMode(BoutonDroit, INPUT);
  pinMode(BoutonHaut, INPUT);
  pinMode(BoutonBas, INPUT);
  //Mise à 0 de toutes les sorties
  digitalWrite(PinVanne,LOW);  //On arrete la vanne
  digitalWrite(PinLumiere,LOW);  //On arrete la lumière
  digitalWrite(PinChauffage,LOW);  //On arrete le chauffage
  //Fermer le volet !!!
  EtatVanne=false;
  EtatLumiere=false;
  EtatChauffage=false;
  EtatVolet=false;
  //initialise les affichages
  
  //initialise l'horloge (en provenance de l'horloge)
  RecupereDateHeure(&DateMesure);
  FaireMesure=true;
  //Lecture de l'eeprom et mise en place des min max
  EEPROM.get(sizeof(int)*4,TrigOuverture);
  EEPROM.get(sizeof(int)*6,TrigHygro);
  EEPROM.get(sizeof(int)*8,TrigTemp);
  EEPROM.get(sizeof(int)*7,TrigLumiere);
  ValeurMenu[MTrigOuv]=TrigOuverture;
  ValeurMenu[MTrigHygro]=TrigHygro;
  ValeurMenu[MTrigLum]=TrigLumiere;
  ValeurMenu[MTrigTemp]=TrigTemp;
  //Lecture de l'intervalle de mesure  
  EEPROM.get(sizeof(int)*9,TempoTrig);
  ValeurMenu[Mtempo]=TempoTrig;
 //Initialise le menu
  InMenu=false;
  IdHorsMenu=0;
  EntreeMenu=0;
  IdMenu=0;
  //Demande a faire une mesure : initialise donc le cycle
  DebutScript=true;
  //Initialise le pointeur d'eeprom
  PointeurEEPROM=0;
  NombreRecord=0;  
  //Initialise le port Série
  Serial.begin(PortSpeed);
  //Initialise l'écran
  MonEcran.begin(16,2); 
  MonEcran.noBacklight();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                 Boucle principale                                               */
/*              On releve la date/heure et on regarde si on doit faire une mesure, puis on         */
/*              on regarde si on a appuyer sur une touche haut ou bas.                             */
/*              Ensuite, on regarde si on doit entrer dans le menu. Enfin on repart au début       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void loop() {
  //Boucle principale.
  byte HeureActu;
  //Faire mesure est calculée en fonction de la l'heure de la dernière mesure
  //On va lire l'heure sur la RTC Puis comparer avec le trig      
  RecupereDateHeure(&DateMesure); 
  if  (DateMesure.heures>=HeureMesure)
  {
    FaireMesure=true; 
  }
  //Scan pour éteindre l'écran, si on a dépassé le temps, on éteint
  if((TempsMilliBackLight+ReinitBackLight)>Maintenant)
  {
    MonEcran.noBacklight();
  }
  if (FaireMesure or DebutScript)
  {
     //Stockage de la date et de l'heure
     MesureFaite.HeureEEPROM=DateMesure.heures;
     MesureFaite.MinuteEEPROM=DateMesure.minutes;
     MesureFaite.JourEEPROM=DateMesure.jour;
     MesureFaite.MoisEEPROM=DateMesure.mois;
     MesureFaite.AnneeEEPROM=DateMesure.annee;
     DebutScript=false;  //On a démarrer la carte, on fait la mesure
     TempInt=MesureTemp(TempIntM);
     TempExt=MesureTemp(TempExtM);
     GestionAreoChauffage(TempInt, TempExt);
     GestionVanne(MesureHygro());
     GestionLumiere(MesureLumiere());
     ReinitialiseMesure(); 
  }
  //Scan des boutons
   bouton_appuye();
   //Scan le port série
   LitSerial();
}
