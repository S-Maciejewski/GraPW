#include<stdio.h>
#include<unistd.h>
#include<stdbool.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<sys/msg.h>
#include<sys/sem.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/ipc.h>

//Parametry jednostek
const double wartosciAtaku[3] = {1.0, 1.5, 3.5};
const double wartosciObrony[3] = {1.2, 3.0, 1.2};
const int czasSzkolenia[4] = {2, 2, 3, 5};
const int koszt[4] = {150, 100, 250, 550};

//Definicja typu GRACZ 
typedef struct gracz {
	long mtype;
	int numer;
	int surowce;
	int jednostki[3];	//0. Lekka piechota, 1. Ciężka piechota, 2. Jazda
	int robotnicy;
	double atak;
	double obrona;
	int zwyciestwa;
	bool wygrana;
} GRACZ; 

struct msg {
	long mtype;
	int numer;
};

struct komunikatAtaku {
	long mtype;
	int jednostki[3];
	int celAtaku;
	int agresor;
};

struct komunikatSzkolenia {
	long mtype; 
	int rodzaj;
	int ilosc;
};

union semun {
	int val;    
   	struct semid_ds *buf;    
   	unsigned short  *array;  
};

void czyGotowi(int kolejka, int semaforGotowosci) {
	struct sembuf semafory = (struct sembuf) {0, 0, 0};
	struct msg gotowy;
	//Czekaj na wiadomość o gotowości gracza
	msgrcv(kolejka, &gotowy, sizeof(gotowy) - sizeof(long), 5, 0); 
	printf("Do gry dolacza gracz %d\n", gotowy.numer+1);

	semafory.sem_op = -1; 
	semop(semaforGotowosci, &semafory, 1);

	//Czekaj na gotowość wszystkich graczy
	semafory.sem_op = 0;
	semop(semaforGotowosci, &semafory, 1);
	//Udziel graczowi zezwolenia na rozpoczęcie gry
	struct msg potwierdzenie = (struct msg) {0};
	//Rozmiar to 0, bo chodzi o rozmiar właściwej treści komunikatu (bez long mtype)
	msgsnd(kolejka, &potwierdzenie, 0, 0); 
}

void kontrolaSurowcow(GRACZ * gracze, int wygrana, int komunikaty, int dostepDoGracza) {
	struct sembuf semafory = (struct sembuf) {0, 0, 0};
	bool * istniejeZwyciezca = shmat(wygrana, 0, 0);
	struct komunikatSzkolenia rekrutacja;
	struct msg zezwolenie = (struct msg){3, 0};

	while(!*istniejeZwyciezca){
		msgrcv(komunikaty, &rekrutacja, sizeof(rekrutacja) - sizeof(long), 6, 0);
		
		semafory.sem_op = -1;
		semop(dostepDoGracza, &semafory, 1);

		if (koszt[rekrutacja.rodzaj] * rekrutacja.ilosc <= gracze->surowce) {
			zezwolenie.numer = 1;
			gracze->surowce -= koszt[rekrutacja.rodzaj] * rekrutacja.ilosc;
		}
		else zezwolenie.numer = 0;

		semafory.sem_op = 1;
		semop(dostepDoGracza, &semafory, 1);

		msgsnd(komunikaty, &zezwolenie, sizeof(zezwolenie) - sizeof(long), 0);
	}
	exit(0);
}

void dodajSurowce(GRACZ * gracze, int wygrana, int komunikaty, int dostepDoGracza){
	struct sembuf semafory = (struct sembuf) {0, 0, 0};
	bool * istniejeZwyciezca = shmat(wygrana, 0, 0);

	msgsnd(komunikaty, gracze, sizeof(*gracze) - sizeof(long), 0);

	while(!*istniejeZwyciezca){
		//Dodawanie surowców co sekundę
		sleep(1);

		semafory.sem_op = -1;
		semop(dostepDoGracza, &semafory, 1);

		gracze->surowce += (50 + gracze->robotnicy * 5);
		msgsnd(komunikaty, gracze, sizeof(*gracze) - sizeof(long), 0);

		semafory.sem_op = 1;
		semop(dostepDoGracza, &semafory, 1);
	}
	exit(0);
}

void obslugaJednostek(GRACZ * gracze, int wygrana, int komunikaty, int dostepDoGracza) {
	struct sembuf semafory = (struct sembuf) {0, 0, 0};
	bool * istniejeZwyciezca = shmat(wygrana, 0, 0);

	struct komunikatSzkolenia rekrutacja;
	struct msg zezwolenie = (struct msg){4, 0};

	while(!*istniejeZwyciezca){
		msgrcv(komunikaty, &rekrutacja, sizeof(rekrutacja) - sizeof(long), 8, 0);
		
		semafory.sem_op = -1;
		semop(dostepDoGracza, &semafory, 1);

		if (rekrutacja.rodzaj == 0 && rekrutacja.ilosc <= gracze->jednostki[0]) zezwolenie.numer = 1;
		else if (rekrutacja.rodzaj == 1 && rekrutacja.ilosc <= gracze->jednostki[1]) zezwolenie.numer = 1;
		else if (rekrutacja.rodzaj == 2 && rekrutacja.ilosc <= gracze->jednostki[2]) zezwolenie.numer = 1;
		//Sprawdzanie czy gracz może zaatakować wybrany cel (czy nie atakuje sam siebie)
		else if (rekrutacja.rodzaj >= 10 && rekrutacja.rodzaj - 10 != rekrutacja.ilosc - 1 && rekrutacja.ilosc <= 3 && rekrutacja.ilosc >=1) zezwolenie.numer = 1;
		else zezwolenie.numer = 0;

		semafory.sem_op = 1;
		semop(dostepDoGracza, &semafory, 1);

		msgsnd(komunikaty, &zezwolenie, sizeof(zezwolenie) - sizeof(long), 0);
	}
	exit(0);
}

void rekrutujJednostki(GRACZ * gracze, int wygrana, int komunikaty, int dostepDoGracza){
	int rodzaj;

	struct sembuf semafory = (struct sembuf) {0, 0, 0};
	bool * istniejeZwyciezca = shmat(wygrana, 0, 0);
	struct komunikatSzkolenia rekrutacja;

	while(!*istniejeZwyciezca){
		msgrcv(komunikaty, &rekrutacja, sizeof(rekrutacja) - sizeof(long), 7, 0);
		rodzaj = rekrutacja.rodzaj;
		for (int i = 0; i < rekrutacja.ilosc; i++)
		{
			sleep(czasSzkolenia[rodzaj]);

			semafory.sem_op = -1;
			semop(dostepDoGracza, &semafory, 1);

			if (rodzaj >= 1) {
				gracze->jednostki[rodzaj - 1] += 1;		
				gracze->atak += wartosciAtaku[rodzaj - 1];
				gracze->obrona += wartosciObrony[rodzaj - 1];
			}
			else gracze->robotnicy += 1;

			msgsnd(komunikaty, gracze, sizeof(*gracze) - sizeof(long), 0);

			semafory.sem_op = 1;
			semop(dostepDoGracza, &semafory, 1);
		}
	}
	exit(0);
}

void obslugaAtaku(GRACZ * gracze[], int wygrana, int komunikaty[], int dostepDoGracza[], int semaforWalki[], int numerGracza) {
	//Wartosci atakPo i obronaPo to wartosci, ktore po walce bedzie mial obronca, a obronaAgresora to wartosc, o ktora zmniejszy sie obrona agresora
	double SA, SO, obronaAgresora, atakPo, obronaPo;
	int agresor, obronca;

	struct sembuf semafory = (struct sembuf) {0, 0, 0};
	bool * istniejeZwyciezca = shmat(wygrana, 0, 0);
	struct komunikatAtaku bitwa;

	while(!*istniejeZwyciezca){
		msgrcv(komunikaty[numerGracza], &bitwa, sizeof(bitwa) - sizeof(long), 9, 0);
		if(fork()==0) {
			agresor = bitwa.agresor;
			obronca = bitwa.celAtaku;
			
			if (!*istniejeZwyciezca)
			printf("Gracz %d wydał rozkaz ataku na gracza %d przy pomocy:\nLekka piechota: %d,\tCiezka piechota: %d,\tJazda: %d\n", 
				agresor+1, obronca+1, bitwa.jednostki[0], bitwa.jednostki[1], bitwa.jednostki[2]);
			
			//Jednostki agresora ruszaja do ataku
			semafory.sem_op = -1;
			semop(dostepDoGracza[agresor], &semafory, 1);

			for(int i=0; i<3; i++) {
				gracze[agresor]->jednostki[i] -= bitwa.jednostki[i];
			}

			//Obliczanie obrony i ataku jednostek, ktore rozpoczely atak
			SA = 0.0; 
			obronaAgresora = 0.0;
			for(int i=0; i<3; i++) {
				obronaAgresora += bitwa.jednostki[i] * wartosciObrony[i];
				SA += bitwa.jednostki[i] * wartosciAtaku[i];		
			}
			gracze[agresor]->obrona -= obronaAgresora;
			gracze[agresor]->atak -= SA;
			//Aktualizacja danych agresora w pamięci współdzielonej
			msgsnd(komunikaty[numerGracza], gracze[agresor], sizeof(*gracze[agresor]) - sizeof(long), 0);

			semafory.sem_op = 1;
			semop(dostepDoGracza[agresor], &semafory, 1);

			//Ruch jednostek
			sleep(5);

			//Tylko jeden gracz na raz może atakować danego obrońcę
			semafory.sem_op = -1;
			semop(semaforWalki[obronca], &semafory, 1); 
			//Blokada jednostek graczy biorących udział w bitwie
			semafory.sem_op = -1;
			semop(dostepDoGracza[obronca], &semafory, 1);
			semafory.sem_op = -1;
			semop(dostepDoGracza[agresor], &semafory, 1);

			SO = gracze[obronca]->obrona; 
			if (SA > SO) {
				//Zwycięstwo agresora
				for (int i = 0; i < 3; i++) {
					gracze[obronca]->jednostki[i] = 0;
				} 
				gracze[obronca]->atak = 0;
				gracze[obronca]->obrona = 0;

				//Powrót jednostek agresora (uwzględnienie strat)
				for (int i = 0; i < 3; i++)
					gracze[agresor]->jednostki[i] += bitwa.jednostki[i] - (int)(bitwa.jednostki[i] * (SO / SA));//Ilość jednostek jako liczba całkowita

				gracze[agresor]->zwyciestwa += 1; 
				
				//Sprawdzenie czy nie nastąpiło zwycięstwo agresora
				if (gracze[agresor]->zwyciestwa == 5 && !*istniejeZwyciezca) {
					gracze[agresor]->wygrana = true;
					*istniejeZwyciezca = true;
					exit(0);
				}
			} else {
				//Przegrana agresora, powrót jednostek i obliczenie strat obrońcy
				for (int i = 0; i < 3; i++) { 
					gracze[agresor]->jednostki[i] += bitwa.jednostki[i] - (int)(bitwa.jednostki[i] * (SA / SO));
				}

				for (int i = 0; i < 3; i++) {
					gracze[obronca]->jednostki[i] -= (int) (gracze[obronca]->jednostki[i] * (SA / SO));
				}

				atakPo = 0.0;
				obronaPo = 0.0;
				for (int i = 0; i < 3; i++) {
					atakPo += gracze[obronca]->jednostki[i] * wartosciAtaku[i];
					obronaPo += gracze[obronca]->jednostki[i] * wartosciObrony[i];
				}
				gracze[obronca]->atak = atakPo;
				gracze[obronca]->obrona = obronaPo;
			}

			//Aktualizacja ataku i obrony agresora
			atakPo = 0.0;
			obronaPo = 0.0;
			for (int i = 0; i < 3; i++) {
				atakPo += gracze[agresor]->jednostki[i] * wartosciAtaku[i];
				obronaPo += gracze[agresor]->jednostki[i] * wartosciObrony[i];
			}
			gracze[agresor]->atak = atakPo;
			gracze[agresor]->obrona = obronaPo;

			msgsnd(komunikaty[numerGracza], gracze[agresor], sizeof(*gracze[agresor]) - sizeof(long), 0);
			msgsnd(komunikaty[obronca], gracze[obronca], sizeof(*gracze[obronca]) - sizeof(long), 0);

			semafory.sem_op = 1;
			semop(semaforWalki[obronca], &semafory, 1);
			semafory.sem_op = 1;
			semop(dostepDoGracza[agresor], &semafory, 1);
			semafory.sem_op = 1;
			semop(dostepDoGracza[obronca], &semafory, 1);

			exit(0);
		}
	}
	exit(0);
}

int main(int argc, char const *argv[]) {
	//Utworzenie kanałów komunikacji z klientami
	int komunikaty[3];
	for(int i=0; i<3; i++) {
		komunikaty[i] = msgget(256+i, IPC_CREAT | 0640);
	}
	//Inicjalizacja i podpięcie pamięci współdzielonej
	GRACZ * gracze[3];
	int daneGracza[3];
	for(int i=0; i<3; i++) {
		daneGracza[i] = shmget(32 + i, sizeof(GRACZ), IPC_CREAT | 0640);
	} 
	for(int i=0; i<3; i++) {
		gracze[i] = shmat(daneGracza[i],0,0);
		*gracze[i] = (GRACZ) {1, i+1, 300, {0, 0, 0}, 0, 0.0, 0.0, 0, false};
	}

	int wygrana = shmget(35, sizeof(bool), IPC_CREAT | 0640);
	bool * istniejeZwyciezca = shmat(wygrana, 0, 0);
	*istniejeZwyciezca = false;

	//Przygotowanie semaforów
	struct sembuf semafory = (struct sembuf) {0, 0, 0};
	union semun setSem; 

	setSem.val = 3;
	int semaforGotowosci = semget(16, 1, IPC_CREAT | 0640);	//Utworzenie semafora
	semctl(semaforGotowosci, 0, SETVAL, setSem);			//Ustawienie wartości semaforów

	setSem.val = 1;
	int dostepDoGracza[3];
	for (int i = 0; i < 3; i++) {
		dostepDoGracza[i] = semget(17 + i, 1, IPC_CREAT | 0640);
		semctl(dostepDoGracza[i], 0, SETVAL, setSem);
	} 

	setSem.val = 1;
	int semaforWalki[3];
	for (int i = 0; i < 3; i++) {
		semaforWalki[i] = semget(20 + i, 1, IPC_CREAT | 0640);
		semctl(semaforWalki[i], 0, SETVAL, setSem); 
	}

	printf("Rozpoczecie gry - oczekiwanie na graczy\n");
	
	//Utworzenie procesów dla wszystkich graczy
	for (int i = 0; i < 3; i++)
	{
		if (fork()==0) {
			czyGotowi(komunikaty[i], semaforGotowosci);

			//Proces odpowiedzialny za akumulację surowców
			if (fork()==0) 
				dodajSurowce(gracze[i], wygrana, komunikaty[i], dostepDoGracza[i]);

			//Proces odpowiedzialny za rekrutowanie jednostek
			if (fork()==0) {
				if (fork()==0) kontrolaSurowcow(gracze[i], wygrana, komunikaty[i], dostepDoGracza[i]);
				else rekrutujJednostki(gracze[i], wygrana, komunikaty[i], dostepDoGracza[i]);
			}

			//Proces odpowiedzialny sprawdzanie ilości jednostek
			if (fork()==0) 
				obslugaJednostek(gracze[i], wygrana, komunikaty[i], dostepDoGracza[i]);

			//Proces odpowiedzialny za obsługę przeprowadzanych ataków
			if (fork()==0) 
				obslugaAtaku(gracze, wygrana, komunikaty, dostepDoGracza, semaforWalki, i);

			exit(0);
		}
	}

	semafory.sem_op = 0;
	semop(semaforGotowosci, &semafory, 1);

	printf("Rozpoczecie gry\n");

	int czasGry = 0;
	while(!*istniejeZwyciezca){
		//Aktywne oczekiwanie na zwycięstwo któregoś z graczy, pomiar czasu
		czasGry+=1;
		sleep(1);
	}

	int win = 4;
	for (int i = 0; i < 3; i++) { 
		if (gracze[i]->wygrana)
			win = i;
	}

	struct msg koniecGry = (struct msg) {2, win}; 
	for (int i = 0; i < 3; ++i) {
		msgsnd(komunikaty[i], &koniecGry, sizeof(koniecGry) - sizeof(long), 0);
	}	
	
	printf("Koniec gry,\nWygral gracz %d po %d sekundach gry\n", win + 1, czasGry);

	//Usuwanie semaforów, kolejek i pamięci
	shmctl(wygrana, IPC_RMID, 0);
	semctl(semaforGotowosci, 0, IPC_RMID);
	for(int i=0; i<3; i++) {
		shmctl(daneGracza[i], IPC_RMID, 0);
		msgctl(komunikaty[i], IPC_RMID, 0);
		semctl(dostepDoGracza[i], 0, IPC_RMID);
		semctl(semaforWalki[i], 0, IPC_RMID);
	}
	getchar();

	return 0;
}


