//------------------------------- Includes -------------------------------

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

//------------------------------- Defines -------------------------------

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

int me = 0;		// Which player we are, one or two
int him = 0;	// Which player they are, one or two

move finalMove;

int nextMoveNum = 0;	// The number of the next move

int playerOneScore = 0, playerTwoScore = 0;					// Scores for the two players
double playerOneTimeLeft = 60.0, playerTwoTimeLeft = 60.0;	// Time left for the two players

int *ourScore, *hisScore;						// Pointers to the scores for quick reference
double *ourTime, *hisTime;						// Pointers to time left for quick reference

dna *myDNA;

int boardWidth;
int boardHeight;
int *gameBoard;

int possibleMovesFound;
move *possibleMoves[MAX_POSSIBLE_MOVES];		// An array to hold all possible moves we find

// Function prototypes

void selectMove();
void readInputFile(const char *fileName);
void runMove(int player, int from_x, int from_y, int to_x, int to_y, int test_only, int *board);
boardEvaluation *evaluateBoard(int *board, move *theMove);
int main(int argc, char** argv);
void printBoard();
int charToColumn(char c);
char columnToChar(int x);
int countLines(int *board, int x, int y);
void generateMoveList();
int xyToIndex(int x, int y);
void copyBoard(int *s, int *d);
void copyMove(move *s, move *d);
move *makeMove(int x_start, int y_start, int x_end, int y_end);
void addPossibleMove(int from_x, int from_y, int to_x, int to_y);
double scoreEvaluation(boardEvaluation *e);
void runMoveWithStruct(int player, move *theMove, int *theBoard);
void loadDNA(char *path);

//------------------------------- Function definitions -------------------------------

// Load DNA from a file

void loadDNA(char *path) {
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

	myDNA->noBasePair = temp;

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

	myDNA->oneBasePair = temp;

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

	myDNA->twoBasePair = temp;


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

	myDNA->threeBasePair = temp;


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

	myDNA->lineLengthBasePair = temp;


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

	myDNA->currentMarginBasePair = temp;

	// We're done

	fclose(in);

	// A debug check

	if (DEBUG) {
		printf("We loaded the following DNA:\n");
		printf("\t     0: %f\n", myDNA->noBasePair);
		printf("\t     1: %f\n", myDNA->oneBasePair);
		printf("\t     2: %f\n", myDNA->twoBasePair);
		printf("\t     3: %f\n", myDNA->threeBasePair);
		printf("\t  line: %f\n", myDNA->lineLengthBasePair);
		printf("\tmargin: %f\n", myDNA->currentMarginBasePair);
		printf("\n");	
	}
}

// Call runMove using the data in a move struct

void runMoveWithStruct(int player, move *theMove, int *theBoard) {
	runMove(player, theMove->from_x, theMove->from_y, theMove->to_x, theMove->to_y, false, theBoard);
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

// A function to copy a move

void copyMove(move *s, move *d) {
	d->from_x = s->from_x;
	d->from_y = s->from_y;
	d->to_x = s->to_x;
	d->to_y = s->to_y;
	d->score = s->score;
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

		// Now, score it

		possibleMoves[i]->score = scoreEvaluation(tempEval);

		// Now free that evaluation

		free(tempEval);

		// Now, see if it is the best one we've found

		if (possibleMoves[i]->score == 7.0) {		// We found a winner, no need to score the rest
			bestIndex = i;
			bestCount = 1;
			break;
		} else if (possibleMoves[i]->score > bestScore) {
			bestCount = 1;
			bestScore = possibleMoves[i]->score;
			bestIndex = i;
		} else if (possibleMoves[i]->score == bestScore) {	// If the scores are the same...
			bestCount++;									// Make a random choice between them
			if ((float) rand() / RAND_MAX <= ((double) (1.0 / (double) bestCount))) {
				bestIndex = i;		// Note, this is biased towards the front
			}
		}
	}

	// Set up the move

	copyMove(possibleMoves[bestIndex], &finalMove);

	free(tempBoard);
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

// A function to read the given input file and set up our situation

void readInputFile(const char *fileName) {
	// Variables we'll need

	FILE *inputFile = null;

	char buffer[80];

	char fromXChar, toXChar;
	char fromYChar, toYChar;

	int got;

	int x, y;

	// Open the input file

	inputFile = fopen(fileName, "r");

	if (inputFile == null) {
		printf("Unable to open input file: '%s'", fileName);
		printf("Error number %d.\n", errno);
		exit(1);
	}

	// Get the first line

	fgets(buffer, 80, inputFile);

	got = sscanf(buffer, "%d %d %d", &me, &boardHeight, &boardWidth);	// Read in the first line

	if (got != 3) {
		printf("Error reading in first line of input.\n");
		exit(1);
	}

	if (DEBUG)
		printf("I'm player %d, board is %dx%d.\n", me, boardWidth, boardHeight);

	// Now that we know the board size, we need to allocate the board
	
	gameBoard = malloc(boardWidth * boardHeight * sizeof(int));
	memset(gameBoard, 0, boardWidth * boardHeight * sizeof(int));

	if (gameBoard == null) {
		printf("Unable to allocate game board.\n");
		exit(1);
	}

//	if (DEBUG) {	// This prints out any spaces that aren't 0 as they should be
//		for (y = 0; y < boardHeight; y++) {
//			for (x = 0; x < boardWidth; x++) {
//				if (gameBoard[xyToIndex(x, y)] != 0)
//					printf("%d, %d was %d\n", x, y, gameBoard[xyToIndex(x, y)]);
//			}
//		}
//	}

//	if (DEBUG)
//		printBoard(gameBoard);

	// Get the second line

	fgets(buffer, 80, inputFile);

	got = sscanf(buffer, "1 %d %lf", &playerOneScore, &playerOneTimeLeft);	// Read in player info

	if (got != 2) {
		printf("Error reading in second line of input.\n");
		exit(1);
	}

	if (DEBUG)
		printf("Player 1 has %d points and %lf seconds.\n", playerOneScore, playerOneTimeLeft);

	// Get the third line

	fgets(buffer, 80, inputFile);

	got = sscanf(buffer, "2 %d %lf", &playerTwoScore, &playerTwoTimeLeft);

	if (got != 2) {
		printf("Error reading in third line of input.\n");
		exit(1);
	}

	if (DEBUG)
		printf("Player 2 has %d points and %lf seconds.\n", playerTwoScore, playerTwoTimeLeft);

	// Now get the rest of the lines

	while (true) {
		int player, from_y, to_y, from_x_num, to_x_num;
		char from_x, to_x;

		if (!fgets(buffer, 80, inputFile)) {	// Get the next line
			if (feof(inputFile)) {
				break;
			} else {
				printf("Error reading line from the file: '%s'.\n", buffer);
				printf("Error number %d.\n", ferror(inputFile));
				exit(1);
			}
		}

		buffer[7] = '\0';	// Cover up the newline

		got = sscanf(buffer, "%d %c%d %c%d", &player, &from_x, &from_y, &to_x, &to_y);

		if (got != 5) {
			printf("Error reading line from the file: '%s'.\n", buffer);
			printf("Only parsed out %d things.\n", got);
			exit(1);
		}

		from_x_num = charToColumn(from_x);
		to_x_num = charToColumn(to_x);
		from_y = from_y - 1;
		to_y = to_y - 1;

		if (DEBUG) {
			printf("Player %d made a move from %d, %d to %d, %d.\n", player, from_x_num, from_y, to_x_num, to_y);
		}

		runMove(player, from_x_num, from_y, to_x_num, to_y, false, gameBoard);
//		printBoard();
	}

	if (DEBUG) {
		boardEvaluation *temp;
		
		printBoard(gameBoard);	// Show the board
		
		temp = evaluateBoard(gameBoard, null);	// Figure out the counts

		printf("Boxes with no lines:     %d\n", temp->noSides);		// Print out the counts
		printf("Boxes with one line:     %d\n", temp->oneSides);
		printf("Boxes with two lines:    %d\n", temp->twoSides);
		printf("Boxes with three lines:  %d\n", temp->threeSides);
		printf("Boxes owned by player 0: %d\n", temp->playerOtherOwned);
		printf("Boxes owned by player 1: %d\n", temp->playerOneOwned);
		printf("Boxes owned by player 2: %d\n", temp->playerTwoOwned);
		printf("Winner is: ");

		if (temp->winner == PLAYER_ONE)
			printf("Player 1\n");
		else if (temp->winner == PLAYER_TWO)
			printf("Player 2\n");
		else if (temp->winner == PLAYER_TIE)
			printf("Game was a tie\n");
		else if (temp->winner == NO_WINNER_YET)
			printf("None yet\n");
		else {
			printf("Unknown winner for board: %d\n", temp->winner);
			exit(1);
		}
		
		printf("\n");

		free(temp);									// Free the object that was given to us
	}

	// That takes care of all input, so close the file.

	fclose(inputFile);

	// Set some quick stuff up

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
			if (x != boardWidth)
				board[xyToIndex(x,i)] = board[xyToIndex(x, i)] | LEFT_LINE;
			// Mark the right line if needed
			if (x != 0)
				board[xyToIndex(x - 1, i)] = board[xyToIndex(x - 1, i)] | RIGHT_LINE;
		}
	} else {
		// It's a horizontal line

		y = from_y;

		for (i = from_x; i < to_x; i++) {
			// Mark the top line if needed
			if (y != boardHeight)
				board[xyToIndex(i, y)] = board[xyToIndex(i, y)] | TOP_LINE;
			// Mark the bottom line if needed
			if (y != 0)
				board[xyToIndex(i, y - 1)] = board[xyToIndex(i, y - 1)] | BOTTOM_LINE;
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

// A debug function to print out the game board for us

void printBoard(int *board) {
	int x, y, outputLine, c;

	printf("\n");

	// Draw the board (except the last line)

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
}

// The main function. All hail main!

int main(int argc, char** argv) {

	char fromXChar, toXChar, fromYChar, toYChar;
	int useIPC;
	ipc_memory *ipc;

	myDNA = malloc(sizeof(dna));

	if (myDNA == null) {
		printf("Unable to allocate memory for my DNA!\n");
		exit(1);
	}

	myDNA->noBasePair = 0.984120;	// After 47 evolutions
	myDNA->oneBasePair = 0.576126;
	myDNA->twoBasePair = 0.315090;
	myDNA->threeBasePair = -0.972065;
	myDNA->lineLengthBasePair = 0.020435;
	myDNA->currentMarginBasePair = 0.660055;

	// Initial stuff

	gameBoard = null;
	possibleMovesFound = 0;
	useIPC = 0;

	if (DEBUG) {
		printf("\n");
	}

	// Make sure we have arguments

	if ((argc < 2) || (argc > 4)) {
		printf("Error: bad command line arguments. Please call as:\n");

		printf("\t/path/to/program /path/to/input [/path/to/output] [/path/to/dna]\n");
		printf("\t/path/to/program --ipc key_number\n");

		exit(1);
	}

	if (strncmp(argv[1], "--ipc", 5) == 0) {
		if (argc == 3) {
			// Key is in the command line, so let's get it

			if(sscanf(argv[2], "%d", &useIPC) != 1) {
				// We couldn't load it

				printf("Unable to load the IPC key. Given '%s'.\n", argv[3]);
				exit(1);
			}
		} else {
			// We didn't get enough info

			printf("Error: bad command line arguments for IPC. Please call as:\n");
	
			printf("\t/path/to/program /path/to/input [/path/to/output] [/path/to/dna]\n");
			printf("\t/path/to/program --ipc key_number\n");
	
			exit(1);
		}
	}

	// Read the input file

	if (useIPC) {
		// Set up the IPC shared memory

		ipc = null;

		ipc = (ipc_memory *) shmat(useIPC, 0, 0);

		if (ipc == (ipc_memory *) -1) {
			printf("Unable to get shared memory: error %d\n", errno);
			exit(1);
		}

		// Now set the stuff that readInputFile would do for us

		me = ipc->player;
		boardHeight = ipc->height;
		boardWidth = ipc->width;
		gameBoard = (int *) &(ipc->gameBoard);

		playerOneScore = ipc->pOneScore;
		playerOneTimeLeft = ipc->pOneTime;

		playerTwoScore = ipc->pTwoScore;
		playerTwoTimeLeft = ipc->pTwoTime;

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
	} else {
		readInputFile(argv[1]);
	}

	// Read our DNA if they gave it to us

	if (useIPC) {
		// We get the DNA through the IPC
		myDNA = &(ipc->theDNA);
	} else {
		// Load the DNA from a file if given
		if (argc == 4) {
			loadDNA(argv[3]);
		}
	}

	// Initialize other stuff

	memset(possibleMoves, 0, MAX_POSSIBLE_MOVES * sizeof(move *));	// Clear out the possible moves array

	// Generate a list of possible moves
	
	generateMoveList();

	if (DEBUG) {
		printf("We found %d possible moves.\n\n", possibleMovesFound);
	}

	// Seed the RNG

	srand((unsigned) time(NULL));

	// Time to start processing.

	selectMove();	// Figure out our move

	// Print out the move
	if (useIPC) {
		// Since we are using IPC, things are easy

		copyMove(&finalMove, &(ipc->chosenMove));
	} else {

		fromXChar = columnToChar(finalMove.from_x);
		toXChar = columnToChar(finalMove.to_x);
	
		fromYChar = '1' + finalMove.from_y;
		toYChar = '1' + finalMove.to_y;

		if (argc == 2) {
			// Just print out the result
			printf("%c%c %c%c\n", fromXChar, fromYChar, toXChar, toYChar);
		} else {
			// They want our output put into a file, so we'll have to do that.
			FILE *out = null;
	
			out = fopen(argv[2], "w");
	
			if (out == null) {
				// We couldn't open the file, so complain 
				printf("Unable to open output file! Error %d.\n", errno);
				printf("%c%c %c%c\n", fromXChar, fromYChar, toXChar, toYChar);
			} else {
				// We opened the file, write out stuff and quit.
				fprintf(out, "%c%c %c%c\n", fromXChar, fromYChar, toXChar, toYChar);
				fclose(out);
			}
		}
	}

	if (DEBUG) {
		printBoard(gameBoard);
		printf("%c%c %c%c\n", fromXChar, fromYChar, toXChar, toYChar);
	}

	// Clean up the possible move list

	if (possibleMovesFound > 0) {
		int i;
		
		for (i = 0; i < possibleMovesFound; i++) {
			free(possibleMoves[i]);
		}
	}

	// Detatch from the shared memory if we are using it

	if (useIPC) {
		shmdt(ipc);
	}

	// Now return

	return 0;

}
