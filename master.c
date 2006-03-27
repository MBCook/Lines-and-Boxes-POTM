//------------------------------- Includes -------------------------------

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/timeb.h>
#include <unistd.h>

//------------------------------- Defines -------------------------------

#define MUTATION_RATE			0.1

//------------------------------- Constants -------------------------------

#define MAX_POSSIBLE_MOVES		((9 + 9) * 36)

#define MIN_BOARD_SIDE			3
#define MAX_BOARD_SIDE			8

#define NO_WINNER_YET			0

#define PLAYER_OTHER			0
#define PLAYER_ONE				1
#define PLAYER_TWO				2
#define PLAYER_TIE				3

#define OWNED_BY_PLAYER_ONE		128
#define OWNED_BY_PLAYER_TWO		64
#define OWNED_BY_OTHER			32
#define OWNED_BY_NO_ONE			0

#define OWNER_MASK				(OWNED_BY_PLAYER_ONE | OWNED_BY_PLAYER_TWO | OWNED_BY_OTHER)

#define TOP_LINE				1
#define RIGHT_LINE				2
#define BOTTOM_LINE				4
#define LEFT_LINE				8

#define THREE_OPEN_TOP			(RIGHT_LINE | BOTTOM_LINE | LEFT_LINE)
#define THREE_OPEN_RIGHT		(TOP_LINE | BOTTOM_LINE | LEFT_LINE)
#define THREE_OPEN_BOTTOM		(TOP_LINE | RIGHT_LINE | LEFT_LINE)
#define THREE_OPEN_LEFT			(TOP_LINE | RIGHT_LINE | BOTTOM_LINE)

#define TWO_TOP_RIGHT			(TOP_LINE | RIGHT_LINE)
#define TWO_TOP_BOTTOM			(TOP_LINE | BOTTOM_LINE)
#define TWO_TOP_LEFT			(TOP_LINE | LEFT_LINE)
#define TWO_RIGHT_BOTTOM		(RIGHT_LINE | BOTTOM_LINE)
#define TWO_RIGHT_LEFT			(RIGHT_LINE | LEFT_LINE)
#define TWO_BOTTOM_TWO			(BOTTOM_LINE | LEFT_LINE)

#define FULL_BOX				(TOP_LINE | RIGHT_LINE | BOTTOM_LINE | LEFT_LINE)

#define true					1	// When will C finally get a built in true and false?
#define false					0
#define null					0	// And what about null?

//------------------------------- Structs -------------------------------

typedef struct {				// Used to hold the coords of a move
	int from_x;
	int to_x;
	int from_y;
	int to_y;
	double score;				// Used in figuring out which move is best
} move;

typedef struct {				// Used in our scoring function
	double noBasePair;
	double oneBasePair;
	double twoBasePair;
	double threeBasePair;
	double lineLengthBasePair;
	double currentMarginBasePair;
} dna;

typedef struct {				// Used to pass stuff between the parrent process and me
	dna theDNA;
	int gameBoard[MAX_BOARD_SIDE * MAX_BOARD_SIDE];
	move chosenMove;
	int width;
	int height;
	int player;
	int pOneScore;
	int pTwoScore;
	double pOneTime;
	double pTwoTime;
} ipc_memory;

//------------------------------- Global Variables -------------------------------

move tempMove;			// A move structure we'll use

int moveNum;			// The number of the next move
int turn;

int playerOneScore = 0, playerTwoScore = 0;					// Scores for the two players
double playerOneTimeLeft = 60.0, playerTwoTimeLeft = 60.0;	// Time left for the two players

int boardWidth;
int boardHeight;
int *gameBoard;
int *startBoard;

int *winsArray;
int *lossesArray;
int *tiesArray;
double *timeArray;
ipc_memory *ipc;

move *moveList[136];

// Function prototypes

void runMove(int player, int from_x, int from_y, int to_x, int to_y, int test_only, int *board);
int main(int argc, char** argv);
void printBoard();
int charToColumn(char c);
char columnToChar(int x);
int countLines(int *board, int x, int y);
inline int xyToIndex(int x, int y);
void copyBoard(int *s, int *d);
move *makeMove(int x_start, int y_start, int x_end, int y_end);
void runMoveWithStruct(int player, move *theMove, int *theBoard);
void loadDNA(char *path, dna *dest);
void copyDNA(dna *s, dna *d);
int gameIsOver(int *board);
void writeGame(char *fileName);
move *readLastMove(char *fileName);
void clearMoves();
void copyMove(volatile move *s, move *d);
dna *haveSex(dna *a, dna *b);
void setupStartBoard(int *startBoard);

//------------------------------- Function definitions -------------------------------

// Prepare the start board with some random moves on it

void setupStartBoard(int *startBorad) {
	// OK, first things first, do we want the board empty or filled?

	if (rand() >= (RAND_MAX / 2)) {
		// It should have initial moves
		// How many lines do we want to make?

		int c = rand() % 15 + 1;	// Up to 15 lines

		int i, sx, sy, ex, ey;

		for (i = 0; i <= c; i++) {
			if (rand() % 2 == 1) {
				// Virticle line
				sx = rand() % (boardWidth + 1);
				ex = sx;

				sy = rand() % (boardHeight + 1);
				ey = rand() % (boardHeight + 1);

				if (sy > ey) {
					int t = sy;
					sy = ey;
					ey = t;
				}
			} else {
				// Horizontal line
				sy = rand() % (boardHeight + 1);
				ey = sy;

				sx = rand() % (boardWidth + 1);
				ex = rand() % (boardWidth + 1);

				if (sx > ex) {
					int t = sx;
					sx = ex;
					ex = t;
				}
			}

			// Sanity check
	
			if ((sx == ex) && (sy == ey)) {
				// If this happened, God didn't want us to make this line
				// So we do nothing
			} else {
				runMove(PLAYER_OTHER, sx, sy, ex, ey, 0, startBoard);
			}
		}
	}
}

// Simulate sexual reproduction between two parent DNAs with mutation

dna *haveSex(dna *a, dna *b) {
	// First, allocate a new DNA structure for the child

	dna *c = malloc(sizeof(dna));

	if (c == null) {
		printf("Unable to allocate child DNA memory: %d.\n", errno);
		exit(1);
	}

	// Now, copy parent A's DNA into the child

	copyDNA(a, c);

	// Now we go through and randomly replace A's genes with B's

	double r = (((double) rand()) / ((double) RAND_MAX));

	if (r >= 0.5) {
		c->noBasePair = b->noBasePair;
	}

	r = (((double) rand()) / ((double) RAND_MAX));

	if (r >= 0.5) {
		c->oneBasePair = b->oneBasePair;
	}

	r = (((double) rand()) / ((double) RAND_MAX));

	if (r >= 0.5) {
		c->twoBasePair = b->twoBasePair;
	}

	r = (((double) rand()) / ((double) RAND_MAX));

	if (r >= 0.5) {
		c->threeBasePair = b->threeBasePair;
	}

	r = (((double) rand()) / ((double) RAND_MAX));

	if (r >= 0.5) {
		c->lineLengthBasePair = b->lineLengthBasePair;
	}

	r = (((double) rand()) / ((double) RAND_MAX));

	if (r >= 0.5) {
		c->currentMarginBasePair = b->currentMarginBasePair;
	}

	// Now that that's done, let's do some mutation

	double d;

	r = (((double) rand()) / ((double) RAND_MAX));

	if (r <= MUTATION_RATE) {
		// 'Twill be a mutant, it will.

		r = (((double) rand()) / ((double) RAND_MAX));

		if (r <= (1.0 / 6.0)) {
			// This base pair is going to mutate
			d = (((double) rand()) / ((double) RAND_MAX)) * c->noBasePair;	// 0-100% of base pair
	
			if ((((double) rand()) / ((double) RAND_MAX)) >= 0.5) {
				d = d * -1.0;	// Make it negative
			}
	
			c->noBasePair = c->noBasePair + d;	// Do the mutation
	
			if ((c->noBasePair < -6.0) || (c->noBasePair > 6.0)) {
				// Check the bounds
	
				if (c->noBasePair < -6.0) {
					c->noBasePair = -6.0;
				} else {
					c->noBasePair = 6.0;
				}
			}
		}

		if ((r > (1.0 / 6.0)) && (r <= (2.0 / 6.0))) {
			// This base pair is going to mutate
			d = (((double) rand()) / ((double) RAND_MAX)) * c->oneBasePair;	// 0-100% of base pair
	
			if ((((double) rand()) / ((double) RAND_MAX)) >= 0.5) {
				d = d * -1.0;	// Make it negative
			}
	
			c->oneBasePair = c->oneBasePair + d;	// Do the mutation
	
			if ((c->oneBasePair < -6.0) || (c->oneBasePair > 6.0)) {
				// Check the bounds
	
				if (c->oneBasePair < -6.0) {
					c->oneBasePair = -6.0;
				} else {
					c->oneBasePair = 6.0;
				}
			}
		}
	
		if ((r > (1.0 / 6.0)) && (r <= (2.0 / 6.0))) {
			// This base pair is going to mutate
			d = (((double) rand()) / ((double) RAND_MAX)) * c->twoBasePair;	// 0-100% of base pair
	
			if ((((double) rand()) / ((double) RAND_MAX)) >= 0.5) {
				d = d * -1.0;	// Make it negative
			}
	
			c->twoBasePair = c->twoBasePair + d;	// Do the mutation
	
			if ((c->twoBasePair < -6.0) || (c->twoBasePair > 6.0)) {
				// Check the bounds
	
				if (c->twoBasePair < -6.0) {
					c->twoBasePair = -6.0;
				} else {
					c->twoBasePair = 6.0;
				}
			}
		}
	
		r = (((double) rand()) / ((double) RAND_MAX));
	
		if (r <= MUTATION_RATE) {
			// This base pair is going to mutate
			d = (((double) rand()) / ((double) RAND_MAX)) * c->threeBasePair;	// 0-100% of base pair
	
			if ((((double) rand()) / ((double) RAND_MAX)) >= 0.5) {
				d = d * -1.0;	// Make it negative
			}
	
			c->threeBasePair = c->threeBasePair + d;	// Do the mutation
	
			if ((c->threeBasePair < -6.0) || (c->threeBasePair > 6.0)) {
				// Check the bounds
	
				if (c->threeBasePair < -6.0) {
					c->threeBasePair = -6.0;
				} else {
					c->threeBasePair = 6.0;
				}
			}
		}
	
		r = (((double) rand()) / ((double) RAND_MAX));
	
		if (r <= MUTATION_RATE) {
			// This base pair is going to mutate
			d = (((double) rand()) / ((double) RAND_MAX)) * c->lineLengthBasePair;	// 0-100% of base pair
	
			if ((((double) rand()) / ((double) RAND_MAX)) >= 0.5) {
				d = d * -1.0;	// Make it negative
			}
	
			c->lineLengthBasePair = c->lineLengthBasePair + d;	// Do the mutation
	
			if ((c->lineLengthBasePair < -6.0) || (c->lineLengthBasePair > 6.0)) {
				// Check the bounds
	
				if (c->lineLengthBasePair < -6.0) {
					c->lineLengthBasePair = -6.0;
				} else {
					c->lineLengthBasePair = 6.0;
				}
			}
		}
	
		r = (((double) rand()) / ((double) RAND_MAX));
	
		if (r <= MUTATION_RATE) {
			// This base pair is going to mutate
			d = (((double) rand()) / ((double) RAND_MAX)) * c->currentMarginBasePair;	// 0-100% of base pair
	
			if ((((double) rand()) / ((double) RAND_MAX)) >= 0.5) {
				d = d * -1.0;	// Make it negative
			}
	
			c->currentMarginBasePair = c->currentMarginBasePair + d;	// Do the mutation
	
			if ((c->currentMarginBasePair < -6.0) || (c->currentMarginBasePair > 6.0)) {
				// Check the bounds
	
				if (c->currentMarginBasePair < -6.0) {
					c->currentMarginBasePair = -6.0;
				} else {
					c->currentMarginBasePair = 6.0;
				}
			}
		}
	}

	// That's it, return the child.
	
	return c;
}

// Copy a move from one memory location to another

void copyMove(volatile move *s, move *d) {
	d->from_x = s->from_x;
	d->from_y = s->from_y;
	d->to_x = s->to_x;
	d->to_y = s->to_y;
	d->score = s->score;
}

// Copy DNA from one memory location to another

void copyDNA(dna *s, dna *d) {
	d->noBasePair = s->noBasePair;
	d->oneBasePair = s->oneBasePair;
	d->twoBasePair = s->twoBasePair;
	d->threeBasePair = s->threeBasePair;
	d->lineLengthBasePair = s->lineLengthBasePair;
	d->currentMarginBasePair = s->currentMarginBasePair;
}

// A function to clear the list of moves

void clearMoves() {
	if (moveNum > 1) {
		int i;

		for (i = 0; i < moveNum - 1; i++) {
			free(moveList[i]);
			moveList[i] = null;
		}
	}

	moveNum = 1;
}

// A function to read the last move in from a file

move *readLastMove(char *fileName) {
	FILE *temp = null;

	temp = fopen(fileName, "r");

	if (temp == null) {
		printf("Unable to open file '%s': error %d.\n", fileName, errno);
		exit(1);
	}

	char buffer[80];

	if (fgets(buffer, 80, temp) == null) {
		fclose(temp);
		printf("Unable to read from the file: %d.\n", errno);
		exit(1);
	}

	fclose(temp);
/*
	move *tempM = makeMove(charToColumn(buffer[0]), buffer[1] - '1', charToColumn(buffer[3]), buffer[4] - '1');
	
	buffer[5] = '\0';

	char fx, tx, fy, ty;

	fx = columnToChar(tempM->from_x);
	tx = columnToChar(tempM->to_x);
	
	fy = '0' + tempM->from_y;
	ty = '0' + tempM->to_y;

	printf("Read: '%s', see as '%c%c %c%c'\n", buffer, fx, fy, tx, ty);

	return tempM;
*/
	return makeMove(charToColumn(buffer[0]), buffer[1] - '1', charToColumn(buffer[3]), buffer[4] - '1');
}

// A function to write the game out to the given file name

void writeGame(char *fileName) {
	FILE *temp = null;

	if (moveNum >= 136) {
		printf("Ran out of moves! Something has gone wrong!\n");
		exit(1);
	}

	temp = fopen(fileName, "w");

	char fromXChar, toXChar, fromYChar, toYChar;

	if (temp == null) {
		printf("Unable to open file '%s': error %d.\n", fileName, errno);
		exit(1);
	}

	fprintf(temp, "%d %d %d\n", turn, boardHeight, boardWidth);
	fprintf(temp, "1 %d %f\n", playerOneScore, playerOneTimeLeft);
	fprintf(temp, "2 %d %f\n", playerTwoScore, playerTwoTimeLeft);

	int i, p;

	for (i = 1; i < moveNum - 1; i++) {
		fromXChar = columnToChar(moveList[i]->from_x);
		toXChar = columnToChar(moveList[i]->to_x);
	
		fromYChar = '1' + moveList[i]->from_y;
		toYChar = '1' + moveList[i]->to_y;

		if (i % 2 == 1) {
			p = 1;
		} else {
			p = 2;
		}

		fprintf(temp, "%d %c%c %c%c\n", p, fromXChar, fromYChar, toXChar, toYChar);
	}

	fclose(temp);
}

// A function to figure out if the game is over

int gameIsOver(int *board) {
	int x, y;

	int pOne = 0;
	int pTwo = 0;

	for (y = 0; y < boardHeight; y++) {
		for (x = 0; x < boardWidth; x++) {
			if ((gameBoard[xyToIndex(x, y)] & FULL_BOX) != FULL_BOX) {
				return NO_WINNER_YET;
			} else {
				if ((gameBoard[xyToIndex(x, y)] & OWNER_MASK) == OWNED_BY_PLAYER_ONE) {
					pOne++;
				} else if ((gameBoard[xyToIndex(x, y)] & OWNER_MASK) == OWNED_BY_PLAYER_TWO) {
					pTwo++;
				}
			}
		}
	}

//	printf("%d %d\n", pOne, pTwo);

	if (pOne > pTwo)
		return PLAYER_ONE;
	else if (pOne < pTwo)
		return PLAYER_TWO;
	else
		return PLAYER_TIE;
}

// A function to load DNA from a file

void loadDNA(char *path, dna *dest) {
	// Stuff we'll need

	FILE *in = null;
	double temp;
	char buffer[80];

	// Now, the work

	in = fopen(path, "r");

	if (in == null) {
		// Unable to open the file, complain.
		printf("Unable to open our DNA file: error %d.\n", errno);
		exit(1);
	}

	// Now that the file is open, we'll read it

	if (fgets(buffer, 80, in) == null) {
		printf("Unable to read the first base pair of our dna: %d.\n", errno);
		fclose(in);
		exit(1);
	}

	if (sscanf(buffer, "%lf", &temp) != 1) {
		printf("Unable to interpret the first base pair.\n");
		fclose(in);
		exit(1);
	}

	dest->noBasePair = temp;

	if (fgets(buffer, 80, in) == null) {
		printf("Unable to read the second base pair of our dna: %d.\n", errno);
		fclose(in);
		exit(1);
	}

	if (sscanf(buffer, "%lf", &temp) != 1) {
		printf("Unable to interpret the second base pair.\n");
		fclose(in);
		exit(1);
	}

	dest->oneBasePair = temp;

	if (fgets(buffer, 80, in) == null) {
		printf("Unable to read the third base pair of our dna: %d.\n", errno);
		fclose(in);
		exit(1);
	}

	if (sscanf(buffer, "%lf", &temp) != 1) {
		printf("Unable to interpret the third base pair.\n");
		fclose(in);
		exit(1);
	}

	dest->twoBasePair = temp;


	if (fgets(buffer, 80, in) == null) {
		printf("Unable to read the fourth base pair of our dna: %d.\n", errno);
		fclose(in);
		exit(1);
	}

	if (sscanf(buffer, "%lf", &temp) != 1) {
		printf("Unable to interpret the fourth base pair.\n");
		fclose(in);
		exit(1);
	}

	dest->threeBasePair = temp;


	if (fgets(buffer, 80, in) == null) {
		printf("Unable to read the fifth base pair of our dna: %d.\n", errno);
		fclose(in);
		exit(1);
	}

	if (sscanf(buffer, "%lf", &temp) != 1) {
		printf("Unable to interpret the fifth base pair.\n");
		fclose(in);
		exit(1);
	}

	dest->lineLengthBasePair = temp;


	if (fgets(buffer, 80, in) == null) {
		printf("Unable to read the sixth base pair of our dna: %d.\n", errno);
		fclose(in);
		exit(1);
	}

	if (sscanf(buffer, "%lf", &temp) != 1) {
		printf("Unable to interpret the sixth base pair.\n");
		fclose(in);
		exit(1);
	}

	dest->currentMarginBasePair = temp;

	// We're done

	fclose(in);

	// A debug check
}

void runMoveWithStruct(int player, move *theMove, int *theBoard) {
//	printf("Before...\n");
//	printBoard(gameBoard);
	runMove(player, theMove->from_x, theMove->from_y, theMove->to_x, theMove->to_y, false, theBoard);
//	printf("After...\n");
//	printBoard(gameBoard);
//	printf("-------------");
}

// A function to allocate a move for us

move *makeMove(int x_start, int y_start, int x_end, int y_end) {
	move *temp = malloc(sizeof(move));

	if (temp == null) {
		printf("Unable to allocate a new move structure!\n");
		exit(1);
	}

	temp->from_x = x_start;
	temp->from_y = y_start;
	temp->to_x = x_end;
	temp->to_y = y_end;

	temp->score = 0.0;

	return temp;	// The caller must free the structure, it's their job now.
}

// A function to copy a board to another

void copyBoard(int *s, int *d) {
	int i;

	for (i = 0; i < boardWidth * boardHeight; i++)
		d[i] = s[i];
}

// A function to turn an x, y combination to an array indicie

int xyToIndex(int x, int y) {
	int i, j;

	if (x == boardWidth)
		i = x - 1;
	else
		i = x;

	if (y == boardHeight)
		j = y - 1;
	else
		j = y;

	return j * boardWidth + i;
}

// A function to count the number of lines around a given box

int countLines(int *board, int x, int y) {
	switch(board[xyToIndex(x, y)] & FULL_BOX) {
		case FULL_BOX:
			return 4;
		case THREE_OPEN_TOP:
		case THREE_OPEN_RIGHT:
		case THREE_OPEN_BOTTOM:
		case THREE_OPEN_LEFT:
			return 3;
		case TWO_TOP_RIGHT:
		case TWO_TOP_BOTTOM:
		case TWO_TOP_LEFT:
		case TWO_RIGHT_BOTTOM:
		case TWO_RIGHT_LEFT:
		case TWO_BOTTOM_TWO:
			return 2;
		case TOP_LINE:
		case RIGHT_LINE:
		case BOTTOM_LINE:
		case LEFT_LINE:
			return 1;
		default:
			return 0;
	}
}

// A function to turn a char column specifier into a number we can use

int charToColumn(char c) {
	return (((int) c) - ((int) 'A'));
}

// A function to turn a column number we use into a column character that is expected as input

char columnToChar(int x) {
	return (char) (x + (int) 'A');
}

// A function make a move on the game board

void runMove(int player, int from_x, int from_y, int to_x, int to_y, int test_only, int *board) {
	// This function makes a move on the board

	int x, y, i, count, c;

	// First, draw the new line

	if (from_x == to_x) {
		// It's a virticle line

		x = from_x;

		for (i = from_y; i < to_y; i++) {
			// Mark the left line if needed
			if (x != boardWidth) {
//				printf("Pre %d\n", __LINE__ + 2);
//				printf("Adding left side to %d, %d\n", x, i);
				board[xyToIndex(x,i)] = board[xyToIndex(x, i)] | LEFT_LINE;
//				printf("Post %d\n", __LINE__ - 1);
//				printBoard(board);
			}
			// Mark the right line if needed
			if (x != 0) {
//				printf("Pre %d\n", __LINE__ + 2);
//				printf("Adding right side to %d, %d\n", x - 1, i);
				board[xyToIndex(x - 1, i)] = board[xyToIndex(x - 1, i)] | RIGHT_LINE;
//				printf("Post %d\n", __LINE__ - 1);
//				printBoard(board);
			}
		}
	} else {
		// It's a horizontal line

		y = from_y;

		for (i = from_x; i < to_x; i++) {
			// Mark the top line if needed
			if (y != boardHeight) {
//				printf("Pre %d\n", __LINE__ + 2);
//				printf("Adding top side to %d, %d\n", i, y);
				board[xyToIndex(i, y)] = board[xyToIndex(i, y)] | TOP_LINE;
//				printf("Post %d\n", __LINE__ - 1);
//				printBoard(board);
			}
			// Mark the bottom line if needed
			if (y != 0) {
//				printf("Pre %d\n", __LINE__ + 2);
//				printf("Adding bottom side to %d, %d\n", i, y - 1);
				board[xyToIndex(i, y - 1)] = board[xyToIndex(i, y - 1)] | BOTTOM_LINE;
//				printf("Post %d\n", __LINE__ - 1);
//				printBoard(board);
			}
		}
	}

	// Now mark any new boxes with the owner

	for (y = 0; y < boardHeight; y++) {
		for (x = 0; x < boardWidth; x++) {
			c = board[xyToIndex(x, y)];
			if (((c & FULL_BOX) == FULL_BOX) && ((c & OWNER_MASK) == 0)) {
				// It's a new box! Mark it as the correct player
				int bit;

				switch(player) {
					case PLAYER_ONE:
						bit = OWNED_BY_PLAYER_ONE;
						break;
					case PLAYER_TWO:
						bit = OWNED_BY_PLAYER_TWO;
						break;
					case PLAYER_OTHER:
						bit = OWNED_BY_OTHER;
						break;
					default:
						printf("ERROR: Got bad player: %d\n", player);
						exit(1);
				}

				if (test_only == false) {
					board[xyToIndex(x, y)] = c | bit;	// Mark the owner
				}
			}
		}
	}
}

// A debug function to print out the game board for us

void printBoard(int *board) {
	int x, y, outputLine, c;

	printf("\n");

	// Draw the board (except the last line)
/*
	for (outputLine = 0; outputLine < 2 * boardHeight; outputLine++) {
		y = outputLine / 2;

		for (x = 0; x < boardWidth; x++) {
			c = board[xyToIndex(x, y)];
			
			if (outputLine % 2 == false) {
				// This is a horizontal line
				printf("*");	// Put a dot

				if (c & TOP_LINE)
					printf("-");	//	There was a line on top
				else
					printf(" ");	// No line on top
			} else {
				// Print the virticle line if needed
				
				if (c & LEFT_LINE)
					printf("|");	// There was a line on left
				else
					printf(" ");

				// Print the box's owner

				switch(c & OWNER_MASK) {
					case OWNED_BY_PLAYER_ONE:
						printf("1");
						break;
					case OWNED_BY_PLAYER_TWO:
						printf("2");
						break;
					case OWNED_BY_OTHER:
						printf("X");
						break;
					case OWNED_BY_NO_ONE:
						printf(" ");
						break;
					default:
						printf("Recieved an unknown owner! It was %d.\n", c & OWNER_MASK);
						exit(1);
				}
			}
		}

		// Now the ending characters on the line

		if (outputLine % 2 == false) {
			printf("*\n");	// The last dot on thel ine
		} else {
			if (board[xyToIndex(x - 1, y)] & RIGHT_LINE)	// Is the right line set on the last box?
				printf("|");
			printf("\n");
		}
	}

	// Now we draw the last line

	y = boardHeight - 1;

	for (x = 0; x < boardWidth; x++) {
		// This is a horizontal line
		printf("*");	// Put a dot

		if (board[xyToIndex(x, y)] & BOTTOM_LINE)
			printf("-");	//	There was a line on bottom
		else
			printf(" ");	// No line on top
	}
	
	printf("*\n\n");	// Print the last dot, and two new lines.
*/

	for (y = 0; y < boardHeight; y++) {
		for (x = 0; x < boardWidth; x++) {
			printf("*");
			if (board[xyToIndex(x, y)] & TOP_LINE)
				printf("-");
			else
				printf(" ");
			printf("*");
		}

		printf("\n");

		for (x = 0; x < boardWidth; x++) {
			if (board[xyToIndex(x, y)] & LEFT_LINE)
				printf("|");
			else
				printf(" ");

			if ((board[xyToIndex(x, y)] & OWNER_MASK) == OWNED_BY_PLAYER_ONE)
				printf("1");
			else if ((board[xyToIndex(x, y)] & OWNER_MASK) == OWNED_BY_PLAYER_TWO)
				printf("2");
			else if ((board[xyToIndex(x, y)] & OWNER_MASK) == OWNED_BY_OTHER)
				printf("X");
			else
				printf(" ");

			if (board[xyToIndex(x, y)] & RIGHT_LINE)
				printf("|");
			else
				printf(" ");
		}

		printf("\n");

		for (x = 0; x < boardWidth; x++) {
			printf("*");
			if (board[xyToIndex(x, y)] & BOTTOM_LINE)
				printf("-");
			else
				printf(" ");
			printf("*");
		}

		printf("\n");
	}

	printf("\n");
}

// The main function. All hail main!

int main(int argc, char** argv) {

	// Seed the RNG

	srand((unsigned) time(NULL));

	// Based on argv, we have to figure out what we want to do

	if (argc == 1) {
		printf("\nPlease call like: /path/to/master /path/to/lab [m c s]|[i c s]\n\n");
		printf("m - Make DNA, c is the number of DNA files, s is start num\n");
		printf("i - Run a tourney with IPC, using dna numbers starting at s, count c\n\n");
		printf("DNA files are text and end in .DNA\n");
		printf("Tourneys place the starting board in startingBoard.txt,\n");
		printf("\tand a results file in results.html.\n");
		printf("\n");
		
		return 0;
	} else if (argc != 5) {
		printf("Not enough arguments, call the program with no arguments for instructions.\n");
		return 0;
	}

	// So, now we have to figure out which thing they want to do

	if (argv[2][0] == 'm') {
		// They want to make DNA

		int startNum, theCount;
		int got;
		double tempNum;

		// We need to parse some things

		got = sscanf(argv[3], "%d", &theCount);

		if (got == -1) {
			printf("Unable to read 'c'.\n");
			return 1;
		}

		if (theCount <= 0) {
			printf("The count needs to be greater than 0.\n");
			return 1;
		}

		got = sscanf(argv[4], "%d", &startNum);

		if (got == -1) {
			printf("Unable to read 's'.\n");
			return 1;
		}

		if (startNum <= 0) {
			printf("The start needs to be greater than 0.\n");
			return 1;
		}

		// Now that we've got that, let's do our work

		printf("Starting DNA output...\n");

		char buffer[80];
		FILE *theFile;
		int i, j;

		for (i = startNum; i < startNum + theCount; i++) {
			// First, generate the file name that we'll be using

			sprintf(buffer, "%d.dna\0", i);

			// Now, open the file

			theFile = null;
			theFile = fopen(buffer, "w");

			if (theFile == null) {
				printf("Unable to open '%s' for writing DNA: error %d.\n", buffer, errno);
				return 1;
			}
			
			// Now, do the work

			for (j = 0; j < 6; j++) {
				tempNum = ((double) rand()) / ((double) RAND_MAX);	// Number from 0 ot 1
				tempNum = tempNum * 2.0;							// Number from 0 to 2
				tempNum = tempNum - 1.0;							// Number from -1 to 1
				fprintf(theFile, "%f\n", tempNum);
			}

			// Close the file

			fclose(theFile);

			printf("Wrote number %d...\r", i);
		}

		// Done doing that

		printf("Done writing DNA.\n\n");

	} else if (argv[2][0] == 'i') {
		// They want to run a tourney with IPC

		int startNum, theCount;
		int got;
		int useIPC = 0;

		// We need to parse some things

		got = sscanf(argv[3], "%d", &theCount);

		if (got == -1) {
			printf("Unable to read 'c'.\n");
			return 1;
		}

		if (theCount <= 0) {
			printf("The count needs to be greater than 0.\n");
			return 1;
		}

		got = sscanf(argv[4], "%d", &startNum);

		if (got == -1) {
			printf("Unable to read 's'.\n");
			return 1;
		}

		if (startNum <= 0) {
			printf("The start needs to be greater than 0.\n");
			return 1;
		}

		// Prepare the arrays

		winsArray = null;
		lossesArray = null;
		tiesArray = null;

		winsArray = malloc(sizeof(int) * theCount);

		if (winsArray == null) {
			printf("Unable to allocate wins array.\n");
			exit(1);
		} else {
			memset(winsArray, 0, sizeof(int) * theCount);
		}

		lossesArray = malloc(sizeof(int) * theCount);

		if (lossesArray == null) {
			printf("Unable to allocate losses array.\n");
			exit(1);
		} else {
			memset(lossesArray, 0, sizeof(int) * theCount);
		}
		
		tiesArray = malloc(sizeof(int) * theCount);

		if (tiesArray == null) {
			printf("Unable to allocate ties array.\n");
			exit(1);
		} else {
			memset(tiesArray, 0, sizeof(int) * theCount);
		}

		timeArray = malloc(sizeof(double) * theCount);

		if (timeArray == null) {
			printf("Unable to allocate time array.\n");
			exit(1);
		} else {
			memset(timeArray, 0, sizeof(double) * theCount);
		}

		// Now, get the IPC memory

		key_t key = ftok("/tmp", 'I');	// Get a key
		useIPC = shmget(key, sizeof(ipc_memory), 0664 | IPC_CREAT);	// Ask for the memory

		if (useIPC <= 0) {
			printf("Unable to ask for shared memory: error %d.\n", errno);
			exit(1);
		}

		ipc = (ipc_memory *) shmat(useIPC, 0, 0);	// Attach to the memory

		if (ipc == null) {
			printf("Unable to attach to the IPC memory: error %d.\n", errno);
			shmctl(useIPC, IPC_RMID, null);
			exit(1);
		}

		char keyPointer[80];

		sprintf(keyPointer, "%d", useIPC);	// The key we'll be passing

		// First, we'll need an opening board, we'll generate a random size

		boardWidth = rand() % 6 + 3;
		boardHeight = rand() % 6 + 3;

		startBoard = malloc(boardWidth * boardHeight * sizeof(int));

		if (startBoard == null) {
			printf("Unable to allocate memory for the starting board.\n");
			shmdt(ipc);
			shmctl(useIPC, IPC_RMID, null);
			exit(1);
		}

		memset(startBoard, 0, boardWidth * boardHeight * sizeof(int));

		gameBoard = (int *) &(ipc->gameBoard);

		memset(gameBoard, 0, boardWidth * boardHeight * sizeof(int));

		printf("Board will be %d rows, %d columns\n", boardHeight, boardWidth);

		setupStartBoard(startBoard);

//		printBoard(startBoard);

		// Set up the IPC memory

		ipc->width = boardWidth;
		ipc->height = boardHeight;

		// Write out the board to our temp file, and to our starting board file

		writeGame("startingBoard.txt");

		// Prepare the HTML file

		FILE *html = null;

		html = fopen("results.html", "w");

		if (html == null) {
			printf("Unable to open results file: %d\n", errno);
			shmdt(ipc);
			shmctl(useIPC, IPC_RMID, null);
			return 1;
		}

		fprintf(html, "<html><head><title>Tourney!</title></head><body>\n");
		fprintf(html, "<table border=1>\n");
		fprintf(html, "<tr><td>DNA</td>");

		int i, j, k;

		for (i = startNum; i < startNum + theCount; i++) {
			fprintf(html, "<td>%d</td>", i);
		}

		fprintf(html, "<td>Totals</td><td>Points</td><td>Average Game Time</td></tr>\n");

		// Load up all the DNA we'll be needing

		printf("Loading DNA...");

		dna *dnaArray = malloc(sizeof(dna) * theCount);

		if (dnaArray == null) {
			printf("Unable to allocate space for the DNA array. Error %d.\n", errno);
			shmdt(ipc);
			shmctl(useIPC, IPC_RMID, null);
			exit(1);
		}

		char a[80];

		for (i = 0; i < theCount; i++) {
			sprintf(a, "%d.dna", i + startNum);
			loadDNA(a, &(dnaArray[i]));
		}

		printf(" OK\n");

		// Do it!

		struct timeb s, e;
		double timeDiff;
		int tempPid;

		for (i = startNum; i < startNum + theCount; i++) {
			fprintf(html, "<tr><td>%d</td>", i);

			for (j = startNum; j < i; j++) {
				fprintf(html, "<td>&nbsp;</td>");
			}

			for (j = i; j < startNum + theCount; j++) {

				// Prepare things

				int dnaAWins, dnaBWins, ties;

				dnaAWins = 0;
				dnaBWins = 0;
				ties = 0;

				playerOneScore = 0;
				playerTwoScore = 0;
				playerOneTimeLeft = 60.0;
				playerTwoTimeLeft = 60.0;

				double totalATime = 0.0;
				double totalBTime = 0.0;

				moveNum = 1;
				turn = PLAYER_ONE;

				// OK, time to do it

				copyBoard(startBoard, gameBoard);

				clearMoves();

//				printf("Running first game between %d and %d... ", i, j);

//				printBoard(gameBoard);

				// First, A is 1, B is 2

				while(gameIsOver(gameBoard) == NO_WINNER_YET) {

//					printf("Player %d's turn\n", turn);
//					printf("----------------\n");

//					printBoard(gameBoard);

					// Write out the way things are now

					ipc->pOneScore = playerOneScore;
					ipc->pTwoScore = playerTwoScore;
					ipc->pOneTime = playerOneTimeLeft;
					ipc->pTwoTime = playerTwoTimeLeft;
					ipc->player = turn;

					if (turn == PLAYER_ONE) {
						copyDNA(&(dnaArray[i - startNum]), &(ipc->theDNA));
					} else {
						copyDNA(&(dnaArray[j - startNum]), &(ipc->theDNA));
					}

					// Now, run the program, with appropriate inputs

					ftime(&s);

					int forkResult = fork();

					if (forkResult < 0) {
						// Something went wrong
						printf("Unable to fork!\n");
						shmdt(ipc);
						shmctl(useIPC, IPC_RMID, null);
						exit(1);
					} else if (forkResult == 0) {
						// We are the child, run the program
						execl(argv[1], "lab", "--ipc", keyPointer);
						printf("Should never get here!\n");
						shmdt(ipc);
						shmctl(useIPC, IPC_RMID, null);
						exit(1);
					} else {
						// We are the parrent, wait for the child
						wait(&tempPid);
						
						if (WEXITSTATUS(tempPid)) {
							printf("The forked process returned %d.\n", WEXITSTATUS(tempPid));
							shmdt(ipc);
							shmctl(useIPC, IPC_RMID, null);
							exit(1);
						}
					}

					ftime(&e);

					// Things have run, figure out how long it took

					timeDiff = (e.time - s.time) + ((e.millitm - s.millitm) / 1000.0);

					if (turn == PLAYER_ONE) {
						playerOneTimeLeft -= timeDiff;
						totalATime += timeDiff;
					} else {
						playerTwoTimeLeft -= timeDiff;
						totalBTime += timeDiff;
					}

					// Get the move, save it, and run it

					volatile move *tempMove = &(ipc->chosenMove);

					int index = moveNum - 1;

					moveNum++;

					move *lastMove = malloc(sizeof(move));

					if (lastMove == null) {
						printf("Unable to allocate a temporary move. Error %d.\n", errno);
						shmdt(ipc);
						shmctl(useIPC, IPC_RMID, null);
						exit(1);
					}

					copyMove(tempMove, lastMove);

					moveList[index] = lastMove;

//					printf("Wants (%d, %d) to (%d, %d) for move %d\n\n", lastMove->from_x,
//												lastMove->from_y, lastMove->to_x, lastMove->to_y, moveNum);

					runMoveWithStruct(turn, lastMove, gameBoard);

//					printBoard(gameBoard);

					// Change turns

					if (turn == PLAYER_ONE) {
						turn = PLAYER_TWO;
					} else {
						turn = PLAYER_ONE;
					}
				}

//				printBoard(gameBoard);

				int tempIndex;

				for (tempIndex = 0; tempIndex < moveNum - 2; tempIndex++) {
					free(moveList[tempIndex]);
				}

				// OK, that game is over. who won?
				
				int winner = gameIsOver(gameBoard);

				if (winner == PLAYER_ONE) {
//					printf("a wins\n");
					dnaAWins++;
					winsArray[i - startNum]++;
					lossesArray[j - startNum]++;
				} else if (winner == PLAYER_TWO) {
//					printf("b wins\n");
					dnaBWins++;
					winsArray[j - startNum]++;
					lossesArray[i - startNum]++;
				} else {
//					printf("tie\n");
					ties++;
					tiesArray[i - startNum]++;
					tiesArray[j - startNum]++;
				}

//				printBoard(gameBoard);

				// Now, reverse A and B and run again

				playerOneScore = 0;
				playerTwoScore = 0;
				playerOneTimeLeft = 60.0;
				playerTwoTimeLeft = 60.0;

				moveNum = 1;
				turn = PLAYER_ONE;

				// OK, time to do it

				copyBoard(startBoard, gameBoard);

				clearMoves();

//				printf("Running second game between %d and %d... ", i, j);

				// Now A is 2 and B is 1

				while(gameIsOver(gameBoard) == NO_WINNER_YET) {

//					printf("Player %d's turn\n", turn);
//					printf("----------------\n");

//					printBoard(gameBoard);

					// Write out the way things are now

					ipc->pOneScore = playerOneScore;
					ipc->pTwoScore = playerTwoScore;
					ipc->pOneTime = playerOneTimeLeft;
					ipc->pTwoTime = playerTwoTimeLeft;
					ipc->player = turn;

					if (turn == PLAYER_TWO) {
						copyDNA(&(dnaArray[i - startNum]), &(ipc->theDNA));
					} else {
						copyDNA(&(dnaArray[j - startNum]), &(ipc->theDNA));
					}

					// Now, run the program, with appropriate inputs

					ftime(&s);

					int forkResult = fork();

					if (forkResult < 0) {
						// Something went wrong
						printf("Unable to fork!\n");
						shmdt(ipc);
						shmctl(useIPC, IPC_RMID, null);
						exit(1);
					} else if (forkResult == 0) {
						// We are the child, run the program
						execl(argv[1], "lab", "--ipc", keyPointer);
						printf("Should never get here!\n");
						shmdt(ipc);
						shmctl(useIPC, IPC_RMID, null);
						exit(1);
					} else {
						// We are the parrent, wait for the child
						wait(&tempPid);
						
						if (WEXITSTATUS(tempPid)) {
							printf("The forked process returned %d.\n", WEXITSTATUS(tempPid));
							shmdt(ipc);
							shmctl(useIPC, IPC_RMID, null);
							exit(1);
						}
					}

					ftime(&e);

					// Things have run, figure out how long it took

					timeDiff = (e.time - s.time) + ((e.millitm - s.millitm) / 1000.0);

					if (turn == PLAYER_ONE) {
						playerOneTimeLeft -= timeDiff;
						totalBTime += timeDiff;
					} else {
						playerTwoTimeLeft -= timeDiff;
						totalATime += timeDiff;
					}

					// Get the move, save it, and run it

					volatile move *tempMove = &(ipc->chosenMove);

					int index = moveNum - 1;

					moveNum++;

					move *lastMove = malloc(sizeof(move));

					if (lastMove == null) {
						printf("Unable to allocate a temporary move. Error %d.\n", errno);
						shmdt(ipc);
						shmctl(useIPC, IPC_RMID, null);
						exit(1);
					}

					copyMove(tempMove, lastMove);

					moveList[index] = lastMove;

					runMoveWithStruct(turn, lastMove, gameBoard);

					// Change turns

					if (turn == PLAYER_ONE) {
						turn = PLAYER_TWO;
					} else {
						turn = PLAYER_ONE;
					}
				}

				for (tempIndex = 0; tempIndex < moveNum - 2; tempIndex++) {
					free(moveList[tempIndex]);
				}

				// OK, that game is over. who won?
				
				winner = gameIsOver(gameBoard);

				if (winner == PLAYER_ONE) {
//					printf("b wins\n");
					dnaBWins++;
					winsArray[j - startNum]++;
					lossesArray[i - startNum]++;
				} else if (winner == PLAYER_TWO) {
//					printf("a wins\n");
					dnaAWins++;
					winsArray[i - startNum]++;
					lossesArray[j - startNum]++;
				} else {
//					printf("tie\n");
					ties++;
					tiesArray[i - startNum]++;
					tiesArray[j - startNum]++;
				}

//				printBoard(gameBoard);

				// That's it! We know the result. Print it out to the result file

				int res = 2 * dnaAWins + ties;

				switch (res) {
					case 4:
						fprintf(html, "<td bgcolor=\"#00FF00\">%d</td>", res);
						break;
					case 3:
						fprintf(html, "<td bgcolor=\"#66FF66\">%d</td>", res);
						break;
					case 2:
						fprintf(html, "<td bgcolor=\"#CCFF66\">%d</td>", res);
						break;
					case 1:
						fprintf(html, "<td bgcolor=\"#FF9933\">%d</td>", res);
						break;
					case 0:
						fprintf(html, "<td bgcolor=\"#FF0000\">%d</td>", res);
						break;
					default:
						break;
				}

				timeArray[i - startNum] += totalATime;
				timeArray[j - startNum] += totalBTime;
			}

			fprintf(html, "<td>%d/%d/%d</td>", winsArray[i - startNum], tiesArray[i - startNum], lossesArray[i - startNum]);
			fprintf(html, "<td>%d</td>", winsArray[i - startNum] * 2 + tiesArray[i - startNum]);
			fprintf(html, "<td>%f</td>", timeArray[i - startNum] / ((((double) theCount) + 1.0) * 2.0));
			fprintf(html, "</tr>\n");
		}

		fprintf(html, "</table>\n");
		fprintf(html, "</body></html>\n");

		fclose(html);

		// Now, we're going to write out the CSV file

		html = fopen("results.csv", "w");

		if (html == null) {
			printf("Unable to write out the CSV file: error %d.\n", errno);
		} else {
			fprintf(html, "DNA,Wins,Ties,Losses,Points\n");

			int d;

			for (d = startNum; d < startNum + theCount; d++) {
				fprintf(html, "%d,%d,%d,%d,%d\n", d, winsArray[d - startNum], tiesArray[d - startNum],
						lossesArray[d - startNum], winsArray[d - startNum] * 2 + tiesArray[d - startNum]);
			}

			fclose(html);
		}

		// Clean up

		free(dnaArray);
		shmdt(ipc);
		shmctl(useIPC, IPC_RMID, null);

		// That's it

		printf("Done running tourney.\n");
	}

	return 0;

}
