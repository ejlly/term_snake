#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define LIG 20
#define COL 40
#define FRAME_LENGTH 45000 //in microseconds
#define NB_INPUT 1000 // entier > FRAME_LENGTH qui divise FRAME_LENGTH

int tab[LIG][COL];
char buffer[3];

//Apparence du tableau
char fond1[] = "\033[48;2;100;187;6m \033[m";
char fond2[] = "\033[48;2;22;248;12m \033[m";
char ser[] = "\033[48;2;59;120;125m \033[m";
char bon1[] = "\033[48;2;100;187;6m\033[38;2;205;49;49m#\033[m";
char bon2[] = "\033[48;2;22;248;12m\033[38;2;205;49;49m#\033[m";


int pos_cur_x, pos_cur_y;
int score;
int init; //nombre d'anneaux du serpent à sa naissance

void update_score(){
	printf("\033[%dA\033[%dDscore : %d\n", pos_cur_x + 1, pos_cur_y, ++score);
	pos_cur_x = 0;
	pos_cur_y = 0;
}

void update_tab(int pos_x, int pos_y, char *c){
	if(pos_x > pos_cur_x)
		printf("\033[%dB", pos_x - pos_cur_x);
	else if(pos_x < pos_cur_x)
		printf("\033[%dA", pos_cur_x - pos_x);
	if(pos_y > pos_cur_y)
		printf("\033[%dC", pos_y - pos_cur_y);
	else if(pos_y < pos_cur_y)
		printf("\033[%dD", pos_cur_y - pos_y);
	printf("%s\n", c);

	pos_cur_x = pos_x + 1;
	pos_cur_y = 0;
}

//Apparition du bonbon
void bonbon(){
	int x = rand()%LIG, y = rand()%COL;
	while(tab[x][y] != 0){
		x = rand()%LIG;
		y = rand()%COL;
	}
	tab[x][y] = 2;
	if((x+y)%2 == 0)
		update_tab(x, y, bon1);
	else
		update_tab(x, y, bon2);
}

//Structure du serpent (liste doublement chaînée) avec pointeur sur sa queue
typedef struct Serpent Serpent;
struct Serpent{
	Serpent *next, *prec; 
	int x,y;
};

enum direction {HAUT, GAUCHE, BAS, DROITE}; 
int coords_dir[4][2] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};


Serpent *serpent, *queue; 
enum direction dir = DROITE;

void affiche_serpent(Serpent *serpent){
	while(serpent != NULL){
		printf("(%d %d), ", serpent->x, serpent->y);
		serpent = serpent->next;
	}
	printf("\n");
}

void add_top(Serpent **serpent, int new_x, int new_y){
	Serpent *new_head = (Serpent*)malloc(sizeof(Serpent));
	new_head->x = new_x;
	new_head->y = new_y;

	new_head->next = (*serpent);
	new_head->prec = NULL;

	(*serpent)->prec = new_head;
	(*serpent) = new_head;
}

void serpent_avancer(int new_x, int new_y, bool grandit){
	add_top(&serpent, new_x, new_y);
	if(!grandit){
		tab[queue->x][queue->y] = 0;
		if((queue->x+queue->y)%2 == 0)
			update_tab(queue->x, queue->y, fond1);
		else
			update_tab(queue->x, queue->y, fond2);
		queue = queue->prec;
		free(queue->next);
		queue->next = NULL;
	}
	else
		update_score();
	tab[new_x][new_y] = 1;
	update_tab(new_x, new_y, ser);
}

bool nxt_frame(){
	int new_x = serpent->x + coords_dir[dir][0], new_y = serpent->y + coords_dir[dir][1];
	if(0 > new_x || 0 > new_y || LIG <= new_x || COL <= new_y || (tab[new_x][new_y] == 1 && (queue->x != new_x || queue->y != new_y))){ //MORT
		return false;
	}
	if(init > 0){
		serpent_avancer(new_x, new_y,  true);
		if(tab[new_x][new_y] == 2){
			bonbon();
			init++;
		}
	}
	else{
		if(tab[new_x][new_y] <= 1){
			serpent_avancer(new_x, new_y, false);
		}
		else if(tab[new_x][new_y] == 2){
			serpent_avancer(new_x, new_y, true);
			bonbon();
		}
	}
	return true;
}

//HAUT   : 27 91 65
//GAUCHE : 27 91 68
//BAS    : 27 91 66
//DROITE : 27 91 67

void nxt_input(int fd){
	for(int i = 0; i<NB_INPUT; i++){
		read(fd, buffer, 3);
		usleep(FRAME_LENGTH/NB_INPUT);
	}
	if(buffer[0] == 27 && buffer[1] == 91){
		switch(buffer[2]){
			case 65: if(dir != BAS) dir = HAUT; break;
			case 68: if(dir != DROITE) dir = GAUCHE; break;
			case 66: if(dir != HAUT) dir = BAS; break;
			case 67: if(dir != GAUCHE) dir = DROITE; break;
		}
	}
}

int main(){
    int fd = open("/dev/stdin", O_RDONLY | O_NONBLOCK); //kerboard file descriptor
    
    //printf("file descriptor : %d\n", fd);

    struct termios old_term, new_term;
    
    if(tcgetattr(fd, &old_term) != 0){
		printf("Couldn't get old terminal attributes\n");
		return -1;
    }
    
    new_term = old_term;
    new_term.c_iflag = 0;
    new_term.c_lflag &= ~(ECHO | ICANON);

    if(tcsetattr(fd, TCSANOW, &new_term) != 0 && tcsetattr(fd, ISIG, &new_term) != 0){
		printf("Couldn't modify terminal attributes\n");
		return -1;
    }
    
	printf("\033[?25l"); //rendre le curseur invisible

	//initialisation du tableau et du serpent
	srand(time(NULL));
	printf("score : 2\n");

	for(int i = 0; i<LIG; i++){
		for(int j = 0; j<COL; j++){
			if((i+j)%2==0)
				printf("%s", fond1);
			else
				printf("%s", fond2);
		}
		printf("\n");
	}
	

	serpent = (Serpent*)malloc(sizeof(Serpent)); 
		serpent->prec = NULL;
		serpent->next = NULL;
		serpent->x = LIG/2;
		serpent->y = 0;
	queue = serpent;

	score = 1;

	tab[LIG/2][0] = 1; //initialisation du premier maillon du serpent
	
	printf("\033[%dA\n", LIG/2 + LIG%2 + 1);

	pos_cur_x = LIG/2;
	pos_cur_y = 0;
	
	update_tab(pos_cur_x, pos_cur_y, ser);
	bonbon();

	bool isAlive = true;

	init = 3;

	//initialisation du jeu avec naissance de score 4
	while(isAlive && init > 0){
		nxt_input(fd);
		isAlive = nxt_frame();
		init--;
	}


	while(isAlive){
		nxt_input(fd);
		isAlive = nxt_frame();
	}

	update_tab(LIG + 1, 0, "\nPERDU !!!!");

    //Reset du terminal
	printf("\033[?25h\n"); //rendre le curseur visible
    tcsetattr(fd, 0, &old_term);
    return 0;
}
