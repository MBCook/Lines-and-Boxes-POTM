//------------------------------- Includes -------------------------------

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <unistd.h>

//------------------------------- Defines -------------------------------

#define MUTATION_RATE			0.1

#ifndef DEBUG
	#define DEBUG 0
#else
	#define DEBUG 1
#endif


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

typedef struct {				// Used to hold evaluation results
	int noSides;
	int oneSides;
	int twoSides;
	int threeSides;
	int playerOneOwned;
	int playerTwoOwned;
	int playerOtherOwned;
	int winner;
	int moveLength;
} boardEvaluation;

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

int me = 0;		// Which player we are, one or two
int him = 0;	// Which player they are, one or two

move finalMove;

int nextMoveNum = 0;	// The number of the next move

int *ourScore, *hisScore;						// Pointers to the scores for quick reference
double *ourTime, *hisTime;						// Pointers to time left for quick reference

dna *myDNA;

int possibleMovesFound;
move *possibleMoves[MAX_POSSIBLE_MOVES];		// An array to hold all possible moves we find

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
void saveDNA(char *path, dna *source);
void copyDNA(dna *s, dna *d);
int gameIsOver(int *board);
void writeGame(char *fileName);
move *readLastMove(char *fileName);
void clearMoves();
void copyMove(volatile move *s, move *d);
dna *haveSex(dna *a, dna *b, dna *dest);
void setupStartBoard(int *startBoard);
void selectMove();
boardEvaluation *evaluateBoard(int *board, move *theMove);
void generateMoveList();
void addPossibleMove(int from_x, int from_y, int to_x, int to_y);
double scoreEvaluation(boardEvaluation *e);
void playHalf();
void makeDNA(int theCount, int startNum);
void runTourney(int theCount, int startNum);
void breedingProgram(int theCount, int startNum);
void swapDNA(dna *one, dna *two);
void swapInt(int *one, int *two);
void sortDNAByScore(dna dnaArray[], int dnaScores[], int dnaNumbers[], int theCount);
void makeRandomDNA(dna *dest);

//------------------------------- Function definitions -------------------------------

// Put random genes into DNA

void makeRandomDNA(dna *dest) {

	double tempNum;
	
	tempNum = ((double) rand()) / ((double) RAND_MAX);	// Number from 0 ot 1
	tempNum = tempNum * 2.0;							// Number from 0 to 2
	tempNum = tempNum - 1.0;							// Number from -1 to 1

	dest->noBasePair = tempNum;
	
	tempNum = ((double) rand()) / ((double) RAND_MAX);	// Number from 0 ot 1
	tempNum = tempNum * 2.0;							// Number from 0 to 2
	tempNum = tempNum - 1.0;							// Number from -1 to 1

	dest->oneBasePair = tempNum;
	
	tempNum = ((double) rand()) / ((double) RAND_MAX);	// Number from 0 ot 1
	tempNum = tempNum * 2.0;							// Number from 0 to 2
	tempNum = tempNum - 1.0;							// Number from -1 to 1

	dest->twoBasePair = tempNum;
	
	tempNum = ((double) rand()) / ((double) RAND_MAX);	// Number from 0 ot 1
	tempNum = tempNum * 2.0;							// Number from 0 to 2
	tempNum = tempNum - 1.0;							// Number from -1 to 1

	dest->threeBasePair = tempNum;
	
	tempNum = ((double) rand()) / ((double) RAND_MAX);	// Number from 0 ot 1
	tempNum = tempNum * 2.0;							// Number from 0 to 2
	tempNum = tempNum - 1.0;							// Number from -1 to 1

	dest->lineLengthBasePair = tempNum;
	
	tempNum = ((double) rand()) / ((double) RAND_MAX);	// Number from 0 ot 1
	tempNum = tempNum * 2.0;							// Number from 0 to 2
	tempNum = tempNum - 1.0;							// Number from -1 to 1

	dest->currentMarginBasePair = tempNum;
}

// Sort a bunch of DNA decending by score

void sortDNAByScore(dna dnaArray[], int dnaScores[], int dnaNumbers[], int theCount) {
	// Use bubble sort

	int i, j;
	int swapHappened;

	for (i = 0; i < theCount - 1; i++) {
		swapHappened = false;	// For checking if a swap occured

		for (j = 0; j < theCount - i - 1; j++) {
			if (dnaScores[j] < dnaScores[j + 1]) {
				// They need to be swapped

				swapHappened = true;	// Mark that we swapped

				swapDNA(&dnaArray[j], &dnaArray[j + 1]);
				swapInt(&dnaScores[j], &dnaScores[j + 1]);
				swapInt(&dnaNumbers[j], &dnaNumbers[j + 1]);
			}
		}

		if (swapHappened == false) {
			break;	// We're done since there were no swaps, so exit
		}
	}
}

// Swap two DNA bits

void swapDNA(dna *one, dna *two) {
	dna temp;

	copyDNA(one, &temp);
	copyDNA(two, one);
	copyDNA(&temp, two);
}

void swapInt(int *one, int *two) {
	int temp;

	temp = *one;
	*one = *two;
	*two = temp;
}

// Using our magic DNA

double scoreEvaluation(boardEvaluation *e) {
	// First, some variables we'll need

	double squareCount;
	double temp;
	int margin;
	double score;

	// First a quick check to see if we found a winner

	if (e->winner != NO_WINNER_YET) {
		if (e->winner == me) {
			return 7.0;				// 6.0 is the highest possible score, so this guarantees selection
		} else {
			return -6.0;			// The lowest possible valid score
		}
	}

	// Initialize things

	squareCount = boardWidth * boardHeight;
	score = 0.0;

	// Now, we do the calculations
	// First, look at who is winning and by how much

	margin = *ourScore - *hisScore;
	temp = ((double) margin) / squareCount;

	score += temp * myDNA->currentMarginBasePair;

	// Now, look at how many squares have no lines on them

	temp = ((double) e->noSides) / squareCount;
	score += temp * myDNA->noBasePair;

	// Now, look at how many squares have one line on them

	temp = ((double) e->oneSides) / squareCount;
	score += temp * myDNA->oneBasePair;

	// Now, look at how many squares have two lines on them

	temp = ((double) e->twoSides) / squareCount;
	score += temp * myDNA->twoBasePair;

	// Now, look at how many squares have three lines on them

	temp = ((double) e->threeSides) / squareCount;
	score += temp * myDNA->threeBasePair;

	// Now, how long was the last move (1 section, 3, 12, etc);

	temp = ((double) e->moveLength) / 9.0;	// 9 segments is the longest possible line
	score += temp * myDNA->lineLengthBasePair;

	// That's it

	return score;
}

// A function to add a possible move to the list

void addPossibleMove(int from_x, int from_y, int to_x, int to_y) {
/*	if (DEBUG) {
		char fromXChar, fromYChar, toXChar, toYChar;
		fromXChar = columnToChar(from_x);
		toXChar = columnToChar(to_x);
	
		fromYChar = '1' + from_y;
		toYChar = '1' + to_y;
	
		printf("%c%c %c%c\n", fromXChar, fromYChar, toXChar, toYChar);
	}
*/
	possibleMoves[possibleMovesFound++] = makeMove(from_x, from_y, to_x, to_y);
}

// A function to generate a list of all legal moves

void generateMoveList() {
	// First, we'll figure out the horizontal moves that are possible

	int x, y, i, j;
	int start, end;
	move *temp;

	for (y = 0; y < boardHeight; y++) {
		end = -1;
		
		while (end < boardWidth) {
			start = -1;		

			// First, find the first place where we can start a line
			for (x = end + 1; x < boardWidth; x++) {
				if ((gameBoard[xyToIndex(x, y)] & TOP_LINE) == 0) {
					start = x;
					break;
				}
			}

			if (start == -1)
				break;	// No free lines on this line

			// Now that we know where to start, we'll find where to end

			end = boardWidth;	// So if we don't find lines, we have a good endpoint

			for (x = start; x < boardWidth; x++) {
				if (gameBoard[xyToIndex(x, y)] & TOP_LINE) {
					// We found a place with a line! Stop just before it
					end = x;
					break;
				}
			}

			// Now that we've got that, we create the list of possible lines

			for (i = start; i < end; i++) {
				for (j = i + 1; j <= end; j++) {
					addPossibleMove(i, y, j, y);
				}
			}
		}
	}
	
	// We have to handle the last line specially, because we have to check for BOTTOM_LINE, not TOP

	end = -1;

	y = boardHeight - 1;

	while (end < boardWidth) {
		start = -1;

		// First, find the first place where we can start a line
		for (x = end + 1; x < boardWidth; x++) {
			if ((gameBoard[xyToIndex(x, y)] & BOTTOM_LINE) == 0) {
				start = x;
				break;
			}
		}
		
		if (start == -1)
			break;	// No free lines on this line

		// Now that we know where to start, we'll find where to end

		end = boardWidth;	// So if we don't find lines, we have a good endpoint

		for (x = start; x < boardWidth; x++) {
			if (gameBoard[xyToIndex(x, y)] & BOTTOM_LINE) {
				// We found a place with a line! Stop just before it
				end = x;
				break;
			}
		}

		// Now that we've got that, we create the list of possible lines

		for (i = start; i < end; i++) {
			for (j = i + 1; j <= end; j++) {
				addPossibleMove(i, boardHeight, j, boardHeight);
			}
		}
	}

	// Now, the same thing, only for virticle lines

	for (x = 0; x < boardWidth; x++) {
		end = -1;
		
		while (end < boardHeight) {
			start = -1;

			// First, find the first place where we can start a line
			for (y = end + 1; y < boardHeight; y++) {
				if ((gameBoard[xyToIndex(x, y)] & LEFT_LINE) == 0) {
					start = y;
					break;
				}
			}
			
			if (start == -1)
				break;	// No free lines on this line

			// Now that we know where to start, we'll find where to end

			end = boardHeight;	// So if we don't find lines, we have a good endpoint

			for (y = start; y < boardHeight; y++) {
				if (gameBoard[xyToIndex(x, y)] & LEFT_LINE) {
					// We found a place with a line! Stop just before it
					end = y;
					break;
				}
			}

			// Now that we've got that, we create the list of possible lines

			for (i = start; i < end; i++) {
				for (j = i + 1; j <= end; j++) {
					addPossibleMove(x, i, x, j);
				}
			}
		}
	}
	
	// We have to handle the last line specially, because we have to check for BOTTOM_LINE, not TOP

	end = -1;

	x = boardWidth - 1;

	while (end < boardHeight) {
		start = -1;

		// First, find the first place where we can start a line
		for (y = end + 1; y < boardHeight; y++) {
			if ((gameBoard[xyToIndex(x, y)] & RIGHT_LINE) == 0) {
				start = y;
				break;
			}
		}
		
		if (start == -1)
			break;	// No free lines on this line

		// Now that we know where to start, we'll find where to end

		end = boardHeight;	// So if we don't find lines, we have a good endpoint

		for (y = start; y < boardHeight; y++) {
			if (gameBoard[xyToIndex(x, y)] & RIGHT_LINE) {
				// We found a place with a line! Stop just before it
				end = y;
				break;
			}
		}

		// Now that we've got that, we create the list of possible lines

		for (i = start; i < end; i++) {
			for (j = i + 1; j <= end; j++) {
				addPossibleMove(boardWidth, i, boardWidth, j);
			}
		}
	}

	// That's it, the possible move list is full!
}

// A function to evalue a gameboard

boardEvaluation *evaluateBoard(int *board, move *lastMove) {
	// Variables
	
	int x, y, i, o;
	boardEvaluation *temp;

	// Allocate our structure
	
	temp = malloc(sizeof(boardEvaluation));

	// Initialize it

	temp->noSides = 0;
	temp->oneSides = 0;
	temp->twoSides = 0;
	temp->threeSides = 0;
	temp->playerOneOwned = 0;
	temp->playerTwoOwned = 0;
	temp->playerOtherOwned = 0;
	temp->winner = PLAYER_OTHER;
	temp->moveLength = -1;

	// Now we count

	for (y = 0; y < boardHeight; y++) {
		for (x = 0; x < boardWidth; x++) {
			i = countLines(board, x, y);

//			printf("%d, %d: %d with %d lines\n", x, y, gameBoard[xyToIndex(x, y)], i);
			
			switch(i) {
				case 0:
					temp->noSides++;
					break;
				case 1:
					temp->oneSides++;
					break;
				case 2:
					temp->twoSides++;
					break;
				case 3:
					temp->threeSides++;
					break;
				case 4:
					o = board[xyToIndex(x, y)] & OWNER_MASK;

					switch(o) {
						case OWNED_BY_PLAYER_ONE:
							temp->playerOneOwned++;
							break;
						case OWNED_BY_PLAYER_TWO:
							temp->playerTwoOwned++;
							break;
						case OWNED_BY_OTHER:
							temp->playerOtherOwned++;
							break;
						default:
							printf("Recieved an unknown owner! It was %d.\n", o);
							exit(1);
					}
					
					break;
				default:
					printf("Box %d, %d had an invalid number of lines! Got %d.\n", x, y, i);
					exit(1);
			}
		}
	}

	// Now figure out if someone won

	if (temp->playerOneOwned + temp->playerTwoOwned + temp->playerOtherOwned == boardWidth * boardHeight) {
		// The game is over
		if (temp->playerOneOwned > temp->playerTwoOwned) {
			temp->winner = PLAYER_ONE;
		} else if (temp->playerOneOwned == temp->playerTwoOwned) {
			temp->winner = PLAYER_TIE;
		} else {
			temp->winner = PLAYER_TWO;
		}
	} else {
		temp->winner = NO_WINNER_YET;
	}

	if (lastMove != null) {
		temp->moveLength = abs(lastMove->from_x - lastMove->to_x) + abs(lastMove->from_y + lastMove->to_y);
	}

	// Return it, note that the caller MUST FREE THE STRUCTURE

	return temp;
}

// A function to choose which move we want

void selectMove() {
	// First, the two variables we'll use

	int i;
	double bestScore;
	int bestIndex;
	int bestCount;
	int *tempBoard;
	boardEvaluation *tempEval;
	int goodMoves[MAX_POSSIBLE_MOVES];

	tempBoard = malloc(boardWidth * boardHeight * sizeof(int));

	if (tempBoard == null) {
		printf("Unable to allocate space for a temporary game board.\n");
		exit(1);
	}

	// A sanity check

	if (possibleMovesFound == 0) {
		printf("Error! No possible moves found!\n");
		printBoard(gameBoard);
		exit(1);
	}

	// Now the real work

	bestScore = -7.0;	// Lower than the lowest possible score
	bestIndex = -1;
	bestCount = -1;

//	i = rand() % possibleMovesFound;

	for (i = 0; i < possibleMovesFound; i++) {
		// First, get us a temporary copy of the current game board

		copyBoard(gameBoard, tempBoard);

		// Now, run the trial move on it

		runMoveWithStruct(me, possibleMoves[i], tempBoard);

		// Now, evaluate it

		tempEval = evaluateBoard(tempBoard, possibleMoves[i]);
		
		// Now free that evaluation

		free(tempEval);

		// Now, score it

		possibleMoves[i]->score = scoreEvaluation(tempEval);

		// Now, see if it is the best one we've found

		if (possibleMoves[i]->score == 7.0) {		// We found a winner, no need to score the rest
			bestIndex = i;
			bestCount = 1;
			break;
		} else if (possibleMoves[i]->score > bestScore) {
			bestCount = 1;
			bestIndex = i;
			bestScore = possibleMoves[i]->score;
			goodMoves[bestCount - 1] = i;
		} else if (possibleMoves[i]->score == bestScore) {	// If the scores are the same...
			bestCount++;									// Make a random choice between them
			goodMoves[bestCount - 1] = i;
		}
	}

	// If we have many different options, choose one

	if (bestCount > 1) {
		int which = rand() % bestCount;		// Choose a move
		bestIndex = goodMoves[which];		// From the indexes with the highest score
	}

	// Set up the move

	copyMove(possibleMoves[bestIndex], &finalMove);

	free(tempBoard);
}

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

dna *haveSex(dna *a, dna *b, dna *dest) {
	dna *c = null;

	// Did they give us a desintaion?

	if (dest == null) {
		// First, allocate a new DNA structure for the child
		c = (dna *) malloc(sizeof(dna));
	
		if (c == null) {
			printf("Unable to allocate child DNA memory: %d.\n", errno);
			exit(1);
		}
	} else {
		c = dest;
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
	
			if ((c->noBasePair < -1.0) || (c->noBasePair > 1.0)) {
				// Check the bounds
	
				if (c->noBasePair < -1.0) {
					c->noBasePair = -1.0;
				} else {
					c->noBasePair = 1.0;
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
	
			if ((c->oneBasePair < -1.0) || (c->oneBasePair > 1.0)) {
				// Check the bounds
	
				if (c->oneBasePair < -1.0) {
					c->oneBasePair = -1.0;
				} else {
					c->oneBasePair = 1.0;
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
	
			if ((c->twoBasePair < -1.0) || (c->twoBasePair > 1.0)) {
				// Check the bounds
	
				if (c->twoBasePair < -1.0) {
					c->twoBasePair = -1.0;
				} else {
					c->twoBasePair = 1.0;
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
	
			if ((c->threeBasePair < -1.0) || (c->threeBasePair > 1.0)) {
				// Check the bounds
	
				if (c->threeBasePair < -1.0) {
					c->threeBasePair = -1.0;
				} else {
					c->threeBasePair = 1.0;
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
	
			if ((c->lineLengthBasePair < -1.0) || (c->lineLengthBasePair > 1.0)) {
				// Check the bounds
	
				if (c->lineLengthBasePair < -1.0) {
					c->lineLengthBasePair = -1.0;
				} else {
					c->lineLengthBasePair = 1.0;
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
	
			if ((c->currentMarginBasePair < -1.0) || (c->currentMarginBasePair > 1.0)) {
				// Check the bounds
	
				if (c->currentMarginBasePair < -1.0) {
					c->currentMarginBasePair = -1.0;
				} else {
					c->currentMarginBasePair = 1.0;
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

// A function to write out DNA to a file

void saveDNA(char *path, dna *source) {
	FILE *out = null;

	// Open the file

	out = fopen(path, "w");

	if (out == null) {
		// Unable to open the file, complain
		printf("Unable to open the output file: error %d.\n", errno);
		exit(1);
	}

	// Write the info

	fprintf(out, "%lf\n", source->noBasePair);
	fprintf(out, "%lf\n", source->oneBasePair);
	fprintf(out, "%lf\n", source->twoBasePair);
	fprintf(out, "%lf\n", source->threeBasePair);
	fprintf(out, "%lf\n", source->lineLengthBasePair);
	fprintf(out, "%lf\n", source->currentMarginBasePair);

	// Close the file

	fclose(out);
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

// Figure out a move as a DNA person

void playHalf() {

	// Prepare some basic stuff

	possibleMovesFound = 0;

	me = ipc->player;
	boardHeight = ipc->height;
	boardWidth = ipc->width;

	if (me == 1) {
		him = 2;
		ourScore = &playerOneScore;
		ourTime = &playerOneTimeLeft;
		hisScore = &playerTwoScore;
		hisTime = &playerTwoTimeLeft;
	} else {
		him = 1;
		hisScore = &playerOneScore;
		hisTime = &playerOneTimeLeft;
		ourScore = &playerTwoScore;
		ourTime = &playerTwoTimeLeft;	
	}

	myDNA = &(ipc->theDNA);

	// Initialize other stuff

	memset(possibleMoves, 0, MAX_POSSIBLE_MOVES * sizeof(move *));	// Clear out the possible moves array

	// Generate a list of possible moves
	
	generateMoveList();

	// Time to start processing.

	selectMove();	// Figure out our move

	copyMove(&finalMove, &(ipc->chosenMove));

	// Clean up the possible move list

	if (possibleMovesFound > 0) {
		int i;

		for (i = 0; i < possibleMovesFound; i++) {
			free(possibleMoves[i]);
		}
	}

	possibleMovesFound = 0;
}

// Make DNA for us

void makeDNA(int theCount, int startNum) {
	printf("Starting DNA output...\n");

	char buffer[80];
	FILE *theFile;
	int i, j;
	double tempNum;

	for (i = startNum; i < startNum + theCount; i++) {
		// First, generate the file name that we'll be using

		sprintf(buffer, "%d.dna\0", i);

		// Now, open the file

		theFile = null;
		theFile = fopen(buffer, "w");

		if (theFile == null) {
			printf("Unable to open '%s' for writing DNA: error %d.\n", buffer, errno);
			exit(1);
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
}

// Run a tournement

void runTourney(int theCount, int startNum) {
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

	ipc = (ipc_memory *) malloc(sizeof(ipc_memory));	// Ask for the memory

	if (ipc == null) {
		printf("Unable to allocate IPC memory: error %d.\n", errno);
		exit(1);
	}

	// First, we'll need an opening board, we'll generate a random size

	boardWidth = rand() % 6 + 3;
	boardHeight = rand() % 6 + 3;

	startBoard = malloc(boardWidth * boardHeight * sizeof(int));

	if (startBoard == null) {
		printf("Unable to allocate memory for the starting board.\n");
		exit(1);
	}

	memset(startBoard, 0, boardWidth * boardHeight * sizeof(int));

	gameBoard = (int *) &(ipc->gameBoard);

	memset(gameBoard, 0, boardWidth * boardHeight * sizeof(int));

	printf("Board will be %d rows, %d columns\n", boardHeight, boardWidth);

	setupStartBoard(startBoard);

//	printBoard(startBoard);

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
		exit(1);
	}

	fprintf(html, "<html><head><title>Tourney!</title></head><body>\n");
	fprintf(html, "<table border=\"0\">\n");
	fprintf(html, "<tr><td>DNA</td>");

	int i, j, k;

	for (i = startNum; i < startNum + theCount; i++) {
		fprintf(html, "<td>%d<br />%d</td>", i / 10, i % 10);
	}

	fprintf(html, "<td>Totals</td><td>Points</td><td>Average Game Time</td></tr>\n");

	// Load up all the DNA we'll be needing

	printf("Loading DNA...");

	dna *dnaArray = malloc(sizeof(dna) * theCount);

	if (dnaArray == null) {
		printf("Unable to allocate space for the DNA array. Error %d.\n", errno);
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

//			printf("Running first game between %d and %d... ", i, j);

//			printBoard(gameBoard);

			// First, A is 1, B is 2

			while(gameIsOver(gameBoard) == NO_WINNER_YET) {

//				printf("Player %d's turn\n", turn);
//				printf("----------------\n");

//				printBoard(gameBoard);

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

				playHalf();

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
					exit(1);
				}

				copyMove(tempMove, lastMove);

				moveList[index] = lastMove;

//				printf("Wants (%d, %d) to (%d, %d) for move %d\n\n", lastMove->from_x,
//											lastMove->from_y, lastMove->to_x, lastMove->to_y, moveNum);

				runMoveWithStruct(turn, lastMove, gameBoard);

//				printBoard(gameBoard);

				// Change turns

				if (turn == PLAYER_ONE) {
					turn = PLAYER_TWO;
				} else {
					turn = PLAYER_ONE;
				}
			}

			int tempIndex;

			for (tempIndex = 0; tempIndex < moveNum - 2; tempIndex++) {
				free(moveList[tempIndex]);
			}

//			printBoard(gameBoard);

			// OK, that game is over. who won?
			
			int winner = gameIsOver(gameBoard);

			if (winner == PLAYER_ONE) {
//				printf("a wins\n");
				dnaAWins++;
				winsArray[i - startNum]++;
				lossesArray[j - startNum]++;
			} else if (winner == PLAYER_TWO) {
//				printf("b wins\n");
				dnaBWins++;
				winsArray[j - startNum]++;
				lossesArray[i - startNum]++;
			} else {
//				printf("tie\n");
				ties++;
				tiesArray[i - startNum]++;
				tiesArray[j - startNum]++;
			}

//			printBoard(gameBoard);

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

//			printf("Running second game between %d and %d... ", i, j);

			// Now A is 2 and B is 1

			while(gameIsOver(gameBoard) == NO_WINNER_YET) {

//				printf("Player %d's turn\n", turn);
//				printf("----------------\n");

//				printBoard(gameBoard);

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

				playHalf();

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
//				printf("b wins\n");
				dnaBWins++;
				winsArray[j - startNum]++;
				lossesArray[i - startNum]++;
			} else if (winner == PLAYER_TWO) {
//				printf("a wins\n");
				dnaAWins++;
				winsArray[i - startNum]++;
				lossesArray[j - startNum]++;
			} else {
//				printf("tie\n");
				ties++;
				tiesArray[i - startNum]++;
				tiesArray[j - startNum]++;
			}

//			printBoard(gameBoard);

			// That's it! We know the result. Print it out to the result file

			int res = 2 * dnaAWins + ties;

			switch (res) {
				case 4:
					fprintf(html, "<td bgcolor=\"#00FF00\">%d</td>", res);
					break;
				case 3:
					fprintf(html, "<td bgcolor=\"#99FF99\">%d</td>", res);
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

	free(winsArray);
	free(lossesArray);
	free(timeArray);
	free(tiesArray);
	free(dnaArray);
	free(ipc);

	// That's it

	printf("Done running tourney.\n");

}

// Run a breeding program

void breedingProgram(int theCount, int startNum) {

	printf("\nRunning a breeding program...\n\n");

	// First, get the memory we'll need

	dna *oldDNAArray = null;
	oldDNAArray = malloc(sizeof(dna) * theCount);

	if (oldDNAArray == null) {
		printf("Unable to allcoate old DNA array: error %d.\n", errno);
		exit(1);
	}

	dna *newDNAArray = null;
	newDNAArray = malloc(sizeof(dna) * theCount);

	if (newDNAArray == null) {
		printf("Unable to allcoate new DNA array: error %d.\n", errno);
		exit(1);
	}

	int *dnaScores = null;
	dnaScores = malloc(sizeof(int) * theCount);

	if (dnaScores == null) {
		printf("Unable to allcoate DNA scores: error %d.\n", errno);
		exit(1);
	}

	int *dnaNumbers = null;
	dnaNumbers = malloc(sizeof(int) * theCount);

	if (dnaNumbers == null) {
		printf("Unable to allcoate DNA scores: error %d.\n", errno);
		exit(1);
	}

	// Now, load up the DNA and the scores

	printf("Loading the DNA... ");

	int i;
	char buffer[80];

	for (i = 0; i < theCount; i++) {
		sprintf(buffer, "%d.dna", i + startNum);
		loadDNA(buffer, &oldDNAArray[i]);
	}

	printf("OK\n");

	printf("Loading the scores... ");

	int temp1, temp2, temp3, temp4, temp5;
	FILE *scores = null;

	scores = fopen("results.csv", "r");

	if (scores == null) {
		printf("Unable to open the results file: error %d.\n", errno);
		exit(1);
	}

	if (fgets(buffer, 80, scores) == null) {
		printf("Unable to read the first line of the file. error: %d.\n", errno);
		fclose(scores);
		exit(1);
	}

	if (strncmp(buffer, "DNA,Wins,Ties,Losses,Points", 27) != 0) {
		printf("This was not a results.csv file as we expected.\n");
		fclose(scores);
		exit(1);
	}

	for (i = 0; i < theCount; i++) {
		if (fgets(buffer, 80, scores) == null) {
			printf("Unable to read line %d: %d.\n", i + 1, errno);
			fclose(scores);
			exit(1);
		}
	
		if (sscanf(buffer, "%d,%d,%d,%d,%d", &temp1, &temp2, &temp3, &temp4, &temp5) != 5) {
			printf("Unable to interpret line %d.\n", i + 1);
			fclose(scores);
			exit(1);
		}
	
		if (temp1 == i + startNum) {
			dnaScores[i] = temp5;
			dnaNumbers[i] = i + startNum;
		} else {
			printf("Expected to read info for DNA %d, found it for %d.\n", i + startNum, temp1);
			fclose(scores);
			exit(1);
		}
	}

	fclose(scores);

	printf("OK\n");

	// We've loaded everything, now sort it

	sortDNAByScore(oldDNAArray, dnaScores, dnaNumbers, theCount);

	printf("The best were:");

	for (i = 0; i < 10; i++) {
		printf(" %d (%d)", dnaNumbers[i], dnaScores[i]);
		if (i == 5) {
			printf("\n              ");
		}
	}

	printf(".\n");

	// Now that we know that, we do the actual breeding

	/* The plan:
	//
	// DNA #  - Function
	//
	// 1-10   - Lucky breeders (each breeds with 5 good, 1 random)
	// 11-70  - The offspring of above
	// 71-80  - Random losers carried over
	// 81-100 - New random DNA
	//
	*/

	printf("Copying the best... ");

	int outputNum = 0;

	for (outputNum = 0; outputNum < 10; outputNum++) {
		copyDNA(&oldDNAArray[outputNum], &newDNAArray[outputNum]);
	}

	if (outputNum != 10) {
		printf("OutputNum was %d after copying the best. Error?\n", outputNum);
	} else {
		printf("OK\n");
	}

	printf("Breeding... ");

	int p1, p2, p2c;

	for (p1 = 0; p1 < 10; p1++) {	// Choose parrent one
		for (p2c = 0; p2c < 5; p2c++) {	// Parrent two count
			p2 = rand() % 10;			// Choose the parent
			haveSex(&oldDNAArray[p1], &oldDNAArray[p2], &newDNAArray[outputNum]);	// Do it (no pun intended)
			outputNum++;															// Setup next slot
		}
		p2 = rand() % 90 + 10;		// Random parent who was not in the top 10
		haveSex(&oldDNAArray[p1], &oldDNAArray[p2], &newDNAArray[outputNum]);	// Do it (no pun intended)
		outputNum++;															// Setup next slot
	}

	if (outputNum != 70) {
		printf("OutputNum was %d after breeding. Error?\n", outputNum);
	} else {
		printf("OK\n");
	}

	printf("Copying over lucky... ");

	int lucky;
	
	for (i = 0; i < 10; i++) {
		lucky = rand() % 90 + 10;	// Someone who wasn't in the top 10

		copyDNA(&oldDNAArray[lucky], &newDNAArray[outputNum]);
		outputNum++;
	}

	if (outputNum != 80) {
		printf("OutputNum was %d after copying the lucky. Error?\n", outputNum);
	} else {
		printf("OK\n");
	}

	printf("Generating new DNA... ");

	for (i = 0; i < 20; i++) {
		makeRandomDNA(&newDNAArray[outputNum]);
		outputNum++;
	}

	if (outputNum != 100) {
		printf("OutputNum was %d after spontanioius generation. Error?\n", outputNum);
	} else {
		printf("OK\n");
	}

	// Now we save out the new DNA

	printf("Saving the DNA... ");

	for (i = 0; i < theCount; i++) {
		sprintf(buffer, "%d.dna", i + startNum);
		saveDNA(buffer, &newDNAArray[i]);
	}

	printf("OK\n");

	// Incrememnt the generatino count

	printf("Incrementing generation count... ");

	FILE *gCount = null;

	gCount = fopen("count.txt", "r+");

	if (gCount == null) { 
		printf("Unable to open the file: error %d\n", errno);
	} else {

		fseek(gCount, 0, SEEK_SET);

		if (fgets(buffer, 80, gCount) == null) {
			printf("Unable to read count: %d.\n", errno);
		} else  {
			if (sscanf(buffer, "%d", &temp1) != 1) {
				printf("Unable to interpret count.\n");
			} else {
				fseek(gCount, 0, SEEK_SET);

				fprintf(gCount, "%d\n", temp1 + 1);

				printf("#%d complete\n", temp1 + 1);
			}
		}

		fclose(gCount);
	}

	// Now we clean up our memory and return

	printf("\nDone!\n\n");

	free(oldDNAArray);
	free(newDNAArray);
	free(dnaScores);
	free(dnaNumbers);
}

// The main function. All hail main!

int main(int argc, char** argv) {
	// Seed the RNG

	srand((unsigned) time(NULL));

	// Based on argv, we have to figure out what we want to do

	if (argc == 1) {
		printf("\nPlease call like: /path/to/master [m c s]|[i c s]|[b c s]\n\n");
		printf("m - Make DNA, c is the number of DNA files, s is start num\n");
		printf("i - Run a tourney, using dna numbers starting at s, count c\n");
		printf("b - Breed the dna numbers starting at s, count c\n\n");
		printf("DNA files are text and end in .DNA\n");
		printf("Tourneys place the starting board in startingBoard.txt,\n");
		printf("\tand a results file in results.html.\n");
		printf("\n");
		
		return 0;
	} else if (argc != 4) {
		printf("Not enough arguments, call the program with no arguments for instructions.\n");
		return 0;
	}

	// So, now we have to figure out which thing they want to do

	if (argv[1][0] == 'm') {
		// They want to make DNA

		int startNum, theCount;
		int got;
		double tempNum;

		// We need to parse some things

		got = sscanf(argv[2], "%d", &theCount);

		if (got == -1) {
			printf("Unable to read 'c'.\n");
			return 1;
		}

		if (theCount <= 0) {
			printf("The count needs to be greater than 0.\n");
			return 1;
		}

		got = sscanf(argv[3], "%d", &startNum);

		if (got == -1) {
			printf("Unable to read 's'.\n");
			return 1;
		}

		if (startNum <= 0) {
			printf("The start needs to be greater than 0.\n");
			return 1;
		}

		// Now that we've got that, let's do our work

		makeDNA(theCount, startNum);

	} else if (argv[1][0] == 'i') {
		// They want to run a tourney with IPC

		int startNum, theCount;
		int got;

		// We need to parse some things

		got = sscanf(argv[2], "%d", &theCount);

		if (got == -1) {
			printf("Unable to read 'c'.\n");
			return 1;
		}

		if (theCount <= 0) {
			printf("The count needs to be greater than 0.\n");
			return 1;
		}

		got = sscanf(argv[3], "%d", &startNum);

		if (got == -1) {
			printf("Unable to read 's'.\n");
			return 1;
		}

		if (startNum <= 0) {
			printf("The start needs to be greater than 0.\n");
			return 1;
		}

		// Do it

		runTourney(theCount, startNum);

	} else if (argv[1][0] == 'b') {
		// They want to run a tourney with IPC

		int startNum, theCount;
		int got;

		// We need to parse some things

		got = sscanf(argv[2], "%d", &theCount);

		if (got == -1) {
			printf("Unable to read 'c'.\n");
			return 1;
		}

		if (theCount <= 0) {
			printf("The count needs to be greater than 0.\n");
			return 1;
		}

		got = sscanf(argv[3], "%d", &startNum);

		if (got == -1) {
			printf("Unable to read 's'.\n");
			return 1;
		}

		if (startNum <= 0) {
			printf("The start needs to be greater than 0.\n");
			return 1;
		}

		// Do it

		if ((startNum == 1) && (theCount == 100)) {
			breedingProgram(theCount, startNum);
		} else {
			printf("We currently artificially limit you to having to use 1 and 100 for start and count.\n\n");
			exit(1);
		}

	}

	return 0;

}
