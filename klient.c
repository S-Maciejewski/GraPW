#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdbool.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/msg.h>
#include <ncurses.h>

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
	int atakowany;
	int atakujacy;
};

struct komunikatSzkolenia {
	long mtype;
	int rodzaj;
	int ilosc;
};

void printInfo(GRACZ aktualnyGracz) {
	mvprintw(0,0,"Gracz %d\t", aktualnyGracz.numer);
	printw("Surowce: %d\t", aktualnyGracz.surowce);
	printw("Robotnicy: %d\n", aktualnyGracz.robotnicy);
	printw("-------------------------------------\n");
	printw("Jednostka \t      Ilosc\n");
	printw("Lekka piechota  \t%d\n", aktualnyGracz.jednostki[0]);
	printw("Ciezka piechota \t%d\n", aktualnyGracz.jednostki[1]);
	printw("Kawaleria \t\t%d\n", aktualnyGracz.jednostki[2]);
	printw("-------------------------------------\n");

	printw("Zdolnosc ofensywna  : %.2f\n", aktualnyGracz.atak);
	printw("Zdolnosc defensywna : %.2f\n", aktualnyGracz.obrona);

	printw("Wygrane bitwy       : %d\n", aktualnyGracz.zwyciestwa);
	printw("-------------------------------------\n");
	printw("\nAby rekrutowac jednostki wcisnij \'r\', jesli chcesz zlecic atak wcisnij \'a\'\n\n");
	refresh();
}

int main(int argc, char const *argv[]) {
	int numerGracza = *argv[1] - 49;
	initscr();
	noecho();

	//Tworzenie identyfikatora kolejki
	int serverInfo = msgget(256 + numerGracza, IPC_CREAT | 0640);

	//Tworzenie miejsc w pamieci wspoldzielonej	
	int daneGracza = shmget(512 + 16 * numerGracza, sizeof(GRACZ), IPC_CREAT | 0640);
	GRACZ * aktualnyGracz = shmat(daneGracza, 0, 0);
	//Informacja o potencjalnym koncu gry
	int koniecInfo = shmget(512 + 16 * numerGracza + 1, sizeof(bool), IPC_CREAT | 0640);
	bool * koniec = shmat(koniecInfo, 0, 0);
	*koniec = false;
	//Informacja o zezwoleniu na odswiezanie ekranu (czesc stala - ncurses)
	int czyAktualizowac = shmget(512 + 16 * numerGracza + 2, sizeof(bool), IPC_CREAT | 0640);
	bool * aktualizuj = shmat(czyAktualizowac, 0, 0);
	*aktualizuj = true;

	//Wysylanie wiadomosci o gotowosci do gry
	struct msg zawiadomienie = (struct msg) {5, numerGracza};
	msgsnd(serverInfo, &zawiadomienie, sizeof(zawiadomienie) - sizeof(long), 0);//- sizeof(long) bo wysyłamy strukturę bez long mtype, bo jest to typ komunikatu

	printw("Oczekiwanie na pozostalych graczy\n");
	//Oczekiwanie na pozostalych graczy
	refresh();
	//Czekaj na wiadomość o kodzie 0 - gotowość do gry
	msgrcv(serverInfo, NULL, 0, 0, 0);
	clear();

	//Glowny proces odpowiedzialny za komunikacje z uzytkownikiem
	if (fork()==0) {
		int ilosc, wyborJednostki = -1;

		//Przygotowanie komunikatow
		struct msg zezwolenie; 
		struct komunikatSzkolenia szkolenie, wyborJednostek; 
		struct komunikatAtaku rozkazAtaku;

		wyborJednostek.mtype = 8;
		rozkazAtaku.mtype = 9;
		rozkazAtaku.atakujacy = numerGracza;

		char nazwy[4][20] = {"Lekka piechota: ", "Ciezka piechota: ", "Jazda: ", "Numer gracza: "};

		while(!*koniec) {
			char c = getch();	//Odpowiednik getchar() dla ncurses, 'r' oznacza rekrutacje jednostek, 'a' oznacza atak
			if (c == 'r') { 
				//Wylaczenie odswiezania glownego menu i przygotowanie UI dla wyboru jednostek
				*aktualizuj = false;
				clear();
				refresh();

				printw("W twoje surowce w momencie rozpoczecia rekrutcaji to: %d\n", aktualnyGracz->surowce);
				printw("Wybierz rodzaj jednostek do rekrutacji:\n");
				printw("---Nazwa----------------Cena----Atak----Obrona\n");
				printw("0) Robotnicy \t\t150\n1) Lekka piechota \t100\t1\t1.2\n2) Ciezka piechota \t250\t1.5\t3\n3) Jazda \t\t550\t3.5\t1.2\n");
				refresh();
				c = getch();
				clear();
				switch(c) {
					case '0': wyborJednostki = 0; printw("Robotnicy\n"); break;
					case '1': wyborJednostki = 1; printw("Lekka piechota\n"); break;
					case '2': wyborJednostki = 2; printw("Ciezka piechota\n"); break;
					case '3': wyborJednostki = 3; printw("Jazda\n"); break;
					default: wyborJednostki = -1; printw("Nieprawidlowy rodzaj jednostki\n"); break;
				}

				if (wyborJednostki != -1) {
					refresh();
					echo();
					printw("Podaj ilosc: ");
					scanw("%d", &ilosc);

					szkolenie.rodzaj = wyborJednostki;
					szkolenie.ilosc = ilosc;
					szkolenie.mtype = 6;							
					msgsnd(serverInfo, &szkolenie, sizeof(szkolenie) - sizeof(long), 0);
					msgrcv(serverInfo, &zezwolenie, sizeof(zezwolenie) - sizeof(long), 3, 0);

					if (zezwolenie.numer == 1) {
						szkolenie.mtype = 7;
						msgsnd(serverInfo, &szkolenie, sizeof(szkolenie) - sizeof(long), 0);
						printw("\nZlecono produkcje jednostek!\n\n");
					} else 
						printw("\nBrak surowcow potrzebnych aby wyszkolic tyle jedostek\n\n");
					printw("Wcisnij dowolny klawisz aby powrocic do ekranu glownego");
					getch();
					noecho();
				}
				clear();
				printInfo(*aktualnyGracz);
				*aktualizuj = true;

			} else if (c == 'a') {
				*aktualizuj = false;
				echo();
				clear();
				printw("Wprowadz ilosc jednostek danego typu, ktore chcesz wyslac do ataku:\n");
				refresh();

				for(int i=0; i<4; i++) {
					if (i == 3) {
						wyborJednostek.rodzaj = 10 + numerGracza;  //Prostszy sposob zapisu numeru gracza jako rodzaj jednostki
					} else {
						wyborJednostek.rodzaj = i;
					}
					do {
						//Przesuniecie kursora w ncurses do kolejnej linijki i wypisanie nazwy jednostki
						move(i + 1, 0);
						clrtoeol();
						mvprintw(i + 1, 0, "%s: ", nazwy[i]);

						scanw("%d", &ilosc);
						wyborJednostek.ilosc = ilosc;
						msgsnd(serverInfo, &wyborJednostek, sizeof(wyborJednostek) - sizeof(long), 0);
						msgrcv(serverInfo, &zezwolenie, sizeof(zezwolenie) - sizeof(long), 4, 0);
					} while (zezwolenie.numer != 1);

					if (i<=2) {
						rozkazAtaku.jednostki[i] = ilosc;
					}
					else {
						rozkazAtaku.atakowany = ilosc - 1;
					}
				}

				msgsnd(serverInfo, &rozkazAtaku, sizeof(rozkazAtaku) - sizeof(long), 0);
				printw("\nWydano rozkaz ataku gracza %d\n\n", rozkazAtaku.atakowany + 1);
				printw("Wcisnij dowolny klawisz aby powrocic do ekranu glownego");
				getch();
				noecho();

				clear();
				printf("Dane gracza\n");
				printInfo(*aktualnyGracz);
				*aktualizuj = true;
			}
		}
		exit(0);
	}

	//Aktualizacja informacji o graczu - dane pobierane od serwera
	if (fork()==0) {
		while(!*koniec){
			msgrcv(serverInfo, aktualnyGracz, sizeof(*aktualnyGracz) - sizeof(long), 1, 0);
			if(*aktualizuj) {
				printInfo(*aktualnyGracz);
			} 
		}
		//Zakonczenie procesu odswiezania po ukonczeniu gry
		exit(0);
	}


	struct msg win;
	msgrcv(serverInfo, &win, sizeof(win), 2, 0);
	*koniec = true;
	clear();
	printw("Koniec gry\n\n");
	if (numerGracza == win.numer) {
		printw("Wygrales! Jako pierwszy wykonales 5 udanych atakow.");
	} 
	else {
		printw("Wygral gracz %d - jako pierwszy wykonal 5 udanych atakow.", win.numer+1);
	}
	refresh();

	//Usuwanie kolejek na koniec gry
	msgctl(serverInfo, IPC_RMID, 0);
	shmctl(koniecInfo, IPC_RMID, 0);
	shmctl(czyAktualizowac, IPC_RMID, 0);
	shmctl(daneGracza, IPC_RMID, 0);

	getchar();
	return 0;
}

