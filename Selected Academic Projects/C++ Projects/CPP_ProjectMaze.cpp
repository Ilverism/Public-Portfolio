#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector> 

using namespace std;

#define VEC2D_C vector<vector<char>>


//Print Maze
void printMaze(VEC2D_C& mazeData) {

	int l = mazeData.size();
	int w;

	int i, j;

	string mazeStr;

	for (i = 0; i < l; i++) {

		w = mazeData[i].size();
		for (j = 0; j < w; j++) {

			mazeStr += mazeData[i][j];

		}
		mazeStr += '\n';

	}

	cout << mazeStr;

}


// Function to solve the maze using a recursive approach
bool findPath(VEC2D_C &maze,	int x, int y) {

	//Animate the maze
	Sleep(10);
	system("cls");
	printMaze(maze);
	

	//Reached edge of the maze
	if (x < 0 || y < 0 || x >= maze.size() || y >= maze[0].size()) {

		return false;
		
		}

	//Encountered wall
	if (maze[x][y] == '+') {

		return false;

		}

	//Reached destination
	if (maze[x][y] == 'E') {

		cout << endl << "Reached the end of the maze!" << endl;
		//cout << "X: " << x << "     Y: " << y;
		cout << "(" << y << ", " << x << ")";

		return true;
	
		}

	//Mark current position as a wall
	maze[x][y] = '+';

	//Move in all direcitons
	if (findPath(maze, x - 1, y) ||
		findPath(maze, x + 1, y) ||
		findPath(maze, x, y - 1) ||
		findPath(maze, x, y + 1)) {

		return true;
		
		}

	//Backtrack
	maze[x][y] = 'O';

	return false;

	}


//Read maze from file
void readMaze(VEC2D_C &mazeData,	int &width, int &height, int &pos_x, int &pos_y) {
	
	//Open Maze File
	fstream mazeFile;
	string mazeFileName;

	while (true) {

		cout << "Enter the name of the maze file: ";
		cin >> mazeFileName;

		mazeFile.open(mazeFileName);

		if (mazeFile.is_open()) {
			break;
			}

		cout << "File not found! Try again." << endl;

		}

	//Read Maze Data

	mazeFile >> height;
	mazeFile >> width;
	mazeFile >> pos_y;
	mazeFile >> pos_x;

	mazeData.resize(height);
	for (int i = 0; i < height; i++) {

		mazeData[i].resize(width);

		}

	mazeFile.ignore(1);
	for (int i = 0; i < height ; i++) {

		for (int j = 0; j < width ; j++) {

			mazeData[i][j] = mazeFile.get();
			if (mazeData[i][j] == 'S') {
				pos_x = i;
				pos_y = j;
				}

			}

		mazeFile.ignore(999999, '\n');

		}

	printMaze(mazeData);

	}


int main() {

	cout << "[START PROGRAM]" << endl << endl;


	//Create 2D Maze Vector
	int width, height,	pos_x, pos_y;

	VEC2D_C maze;
	readMaze(maze,		width, height, pos_x, pos_y);



	cout << "*Annoying Flickering Text Warning*";
	Sleep(5000);



	if (findPath(maze,   pos_x, pos_y) == false) {

		cout << endl << "Failed to find the edge of the maze.";

		}


	cout << endl << endl << "[END PROGRAM]" << endl << endl;

	}