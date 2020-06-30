#include <iostream>
#include <iomanip>
#include <queue>
#include <list>
#include <string>
#include <math.h>
#include <ctime>
#include <conio.h>
#include <windows.h>
#include <chrono>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>

typedef std::chrono::steady_clock the_clock;

const int width = 1000;
const int height = 1000;
const int numOfThreads = 8;
int xStart = 0;
int	yStart = 0;
int xEnd = 0;
int yEnd = 0;
bool progressisfinished = false;

/*---------------------
  |	   |	 |      |
  |  NW  |  N  |  NE  |
  |X-1Y-1| Y-1 |X+1Y-1|
  ---------------------
  |      |     |      |
  |   W  |  P  |  E   |
  |X-1   |     |  X+1 |
  ---------------------
  |      |     |      |
  |  SW  |  S  |  SE  |
  |X-1Y+1| Y+1 |X+1Y+1|
  ---------------------*/

  // array used in the pathfinidng loop to thread all cardinal directions
int offset[8][2] =
{
	{-1, -1}, //NW
	{ 0, -1}, //N
	{ 1, -1}, //NE
	{ 1,  0}, //E
	{ 1,  1}, //SE
	{ 0,  1}, //S
	{-1,  1}, //SW
	{-1,  0}, //W
};

int progressCounter = 0;
bool foundExit = false;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
//threads used in the init grid function
std::thread threads[numOfThreads];
//threads used in the pathfind function
std::thread paththreads[8];
//thread used for using a condition variable
std::thread progressfinished;

std::mutex mu;
std::mutex mu2;
std::condition_variable cv;

//  ====== CMP201 UNEDITED CODE START======
struct Coord {
	Coord(int nx, int ny)
	{
		x = nx;
		y = ny;
	}

	int x;
	int y;
};
std::list<Coord> coord;

struct node
{
	node()
	{
		fCost = 0;
		gCost = 0;
		hCost = 0;
	}
	node(int x, int y, int xpar, int ypar, int g, int f, int h)
	{
		xPos = x, yPos = y, xParent = xpar, yParent = ypar, fCost = f, gCost = g, hCost = h;
	}
	int weght = 0;
	int xPos, yPos;
	int xParent, yParent;
	int fCost, gCost, hCost;
	bool closed;

	int getFcost() { return fCost; }
	void setFcost(int& xDest, int& yDest)
	{
		fCost = gCost + getHcost(xDest, yDest);
	}
	int getHcost(int& xDest, int& yDest)
	{
		int xD, yD, d;
		xD = xDest - xPos;
		yD = yDest - yPos;

		d = abs(xD) + abs(yD);
		return(d);

	}

};

node gridArray[width][height];
struct priorityOrder {
	;
	bool operator()(node& a, node& b)
	{
		return a.getFcost() > b.getFcost();
	}
};

std::priority_queue<node, std::vector<node>, priorityOrder> openList;

void clearPriorityQueue()
{
	while (!openList.empty())
		openList.pop();
}

//  ====== CMP201 UNEDITED CODE END ======

//function for incrementing the progress count to fulfill the projects mutex requirement
void IncrementProgress()
{
	mu2.lock();
	progressCounter++;
	std::cout << "Progress:" << progressCounter << std::endl;
	mu2.unlock();
}
//function for indicating that the threads have finished and the program can continue fulfilling  the projects syncronization requirement
void FinishedLoading()
{
	std::unique_lock<std::mutex> lck(mu);
	while (!progressisfinished)
	{
		cv.wait(lck);
	}
	std::cout << "Array has been loaded successfully" << std::endl;
}

//threaded version of the grid function
void SingleGridThread(int lowX, int highX, int lowY, int highY)
{
	//X/row first
	for (int j = lowX; j < highX; j++)
	{
		for (int i = lowY; i < highY; i++)
		{
			gridArray[i][j].gCost = 1;
			gridArray[i][j].closed = false;
			gridArray[i][j].xPos = i;
			gridArray[i][j].yPos = j;
		}
	}

	for (int i = 0; i < (highY - lowY) * (highX - lowX) / 6; i++)
	{
		gridArray[rand() % 1 + ((highX - lowX) - 1)][rand() % 1 + ((highY - lowY) - 1)].closed = true;
	}
	IncrementProgress();
}

void initialiseGrid()
{
	int threadnum = 0;
	for (int i = 0; i < numOfThreads; i++)
	{
		threads[i] = std::thread(SingleGridThread, 0, height, i * width / numOfThreads, (i + 1) * width / numOfThreads);
	}

	for (int i = 0; i < numOfThreads; i++)
	{
		threads[i].join();
	}

	progressisfinished = true;

	if (progressCounter == numOfThreads)
	{
		progressfinished = std::thread(FinishedLoading);
		progressfinished.join();
	}
}

//threaded version of the pathfinding search taking in a direction
void SinglePathfindThread(int parentx, int parenty, int offsetx, int offsety, int xStart, int yStart, int xEnd, int yEnd)
{
	gridArray[parentx + offsetx][parenty + offsety].gCost += gridArray[xStart][yStart].gCost;
	gridArray[parentx + offsetx][parenty + offsety].hCost = gridArray[parentx + offsetx][parenty + offsety].getHcost(xEnd, yEnd);
	gridArray[parentx + offsetx][parenty + offsety].setFcost(xEnd, yEnd);
	gridArray[parentx + offsetx][parenty + offsety].xParent = parentx;
	gridArray[parentx + offsetx][parenty + offsety].yParent = parenty;
	mu.lock();
	openList.push(gridArray[parentx + offsetx][parenty + offsety]);
	mu.unlock();
}

void pathfind(int xStart, int yStart, int xEnd, int yEnd)
{
	int x = xStart;
	int y = yStart;

	std::cout << "Start coord: " << xStart << "," << yStart << " - End coord  : " << xEnd << "," << yEnd << std::endl;

	node* start = &gridArray[x][y];

	gridArray[x][y].hCost = start->getHcost(xEnd, yEnd);
	gridArray[x][y].setFcost(xEnd, yEnd);
	gridArray[x][y].fCost = start->getFcost();

	while (!foundExit)
	{
		Coord currentPos = { x, y };
		if (x == xEnd && y == yEnd)
		{
			foundExit = true;
			break;
		}

		int threadnum = 0;
		for (int i = 0; i < 8; i++)
		{
			if (x + offset[i][0] >= 0 && x + offset[i][0] < width && y + offset[i][1] >= 0 && y + offset[i][1] < height
				&& gridArray[x + offset[i][0]][y + offset[i][1]].closed == false)
				paththreads[threadnum++] = std::thread(SinglePathfindThread, x, y, offset[i][0], offset[i][1], xStart, yStart, xEnd, yEnd);
		}

		for (int i = 0; i < threadnum; i++)
		{
			paththreads[i].join();
		}

		gridArray[x][y].closed = true;

		if (!openList.empty()) {
			x = openList.top().xPos;
			y = openList.top().yPos;
			openList.pop();
		}
		coord.push_back(currentPos);
	}
}



int main()
{
	srand(time(NULL));
	int numOfSearches = 0;
	int totaliterations = 5;
	float averagems = 0;
	float averagemsgrid = 0;
	float averagemspath = 0;
	float averagemstotal = 0;
	std::cout << "Commencing " << totaliterations << " iterations for testing on a " << width << " x " << height << " array with: " << numOfThreads << " threads " << std::endl;
	//std::ofstream my_file("aStarsubmission.csv");
	for (int i = 0; i < totaliterations; i++)
	{

		// randomly select start and finish locations
		int xStart = rand() % 5 + 1;
		int	yStart = rand() % 5 + 1;
		int xEnd = width - rand() % 8 - 2;
		int yEnd = height - rand() % 8 - 2;
		clearPriorityQueue();
		the_clock::time_point grid_start = the_clock::now();
		initialiseGrid();
		the_clock::time_point grid_end = the_clock::now();
		the_clock::time_point path_start = the_clock::now();
		pathfind(xStart, yStart, xEnd, yEnd);
		the_clock::time_point path_end = the_clock::now();
		foundExit = false;
		auto time_taken_grid = duration_cast<milliseconds>(grid_end - grid_start).count();
		auto time_taken_path = duration_cast<milliseconds>(path_end - path_start).count();

		char delemeter = ',';
		std::cout << "\nSingle ms for grid init = " << time_taken_grid << std::endl;
		std::cout << "Single ms for pathfind = " << time_taken_path << "\n" << std::endl;

		//my_file << " Iteration: ";
		//my_file << numOfSearches;
		//my_file << " ms: ";
		//my_file << time_taken << std::endl;

		averagemsgrid = averagemsgrid + time_taken_grid;
		averagemspath = averagemspath + time_taken_path;

		progressCounter = 0;
		numOfSearches++;
	}
	std::cout << "Average ms = " << averagemsgrid / totaliterations << std::endl;
	std::cout << "Average ms = " << averagemspath / totaliterations << std::endl;
	system("pause");
}
