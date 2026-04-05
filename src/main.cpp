#include <algorithm>
#include <chrono>
#include <ctime>
#include <graphics.h>
#include <queue>
#include <random>
#include <string>
#include <tchar.h>
#include <vector>
#include <windows.h>

using namespace std;

namespace
{
    constexpr int kMazeRows = 25;
    constexpr int kMazeCols = 25;
    constexpr int kCellSize = 24;
    constexpr int kInfoBarHeight = 80;
    constexpr int kWallThickness = 2;
    constexpr int kMoveCooldownMs = 85;

    constexpr COLORREF kWallColor = RGB(64, 64, 64);
    constexpr COLORREF kFloorColor = RGB(241, 232, 208);
    constexpr COLORREF kPlayerColor = RGB(40, 110, 255);
    constexpr COLORREF kStartColor = RGB(68, 176, 86);
    constexpr COLORREF kGoalColor = RGB(215, 58, 73);
    constexpr COLORREF kHintColor = RGB(245, 208, 66);
    constexpr COLORREF kTextColor = RGB(32, 32, 32);
    constexpr COLORREF kInfoBgColor = RGB(225, 217, 193);

    constexpr int kWindowWidth = kMazeCols * kCellSize;
    constexpr int kWindowHeight = kMazeRows * kCellSize + kInfoBarHeight;

    enum Direction
    {
        Up = 0,
        Right = 1,
        Down = 2,
        Left = 3
    };

    const int kRowOffset[4] = { -1, 0, 1, 0 };
    const int kColOffset[4] = { 0, 1, 0, -1 };
    const Direction kOpposite[4] = { Down, Left, Up, Right };

    struct Cell
    {
        bool walls[4] = { true, true, true, true };
        bool visited = false;
    };

    struct Point
    {
        int row = 0;
        int col = 0;

        bool operator==(const Point& other) const
        {
            return row == other.row && col == other.col;
        }
    };

    class MazeGame
    {
    public:
        MazeGame()
            : maze_(kMazeRows, vector<Cell>(kMazeCols)),
              rng_(static_cast<unsigned int>(time(nullptr))),
              startPoint_{ 0, 0 },
              goalPoint_{ kMazeRows - 1, kMazeCols - 1 }
        {
            resetGame();
        }

        void run()
        {
            initgraph(kWindowWidth, kWindowHeight);
            BeginBatchDraw();

            while (running_)
            {
                handleInput();
                draw();
                Sleep(16);
            }

            EndBatchDraw();
            closegraph();
        }

    private:
        vector<vector<Cell>> maze_;
        mt19937 rng_;
        Point player_;
        Point startPoint_;
        Point goalPoint_;
        vector<Point> shortestPath_;

        bool running_ = true;
        bool showHint_ = false;
        bool gameWon_ = false;
        int steps_ = 0;

        chrono::steady_clock::time_point startTime_;
        chrono::steady_clock::time_point winTime_;
        chrono::steady_clock::time_point lastMoveTime_ = chrono::steady_clock::now();

        void resetGame()
        {
            for (auto& row : maze_)
            {
                for (auto& cell : row)
                {
                    cell = Cell();
                }
            }

            generateMaze();
            player_ = startPoint_;
            steps_ = 0;
            showHint_ = false;
            gameWon_ = false;
            startTime_ = chrono::steady_clock::now();
            winTime_ = startTime_;
            lastMoveTime_ = chrono::steady_clock::now();
            updateShortestPath();
        }

        void generateMaze()
        {
            generateByDFS(startPoint_.row, startPoint_.col);
        }

        void generateByDFS(int row, int col)
        {
            maze_[row][col].visited = true;

            vector<int> directions = { Up, Right, Down, Left };
            shuffle(directions.begin(), directions.end(), rng_);

            for (int dir : directions)
            {
                const int nextRow = row + kRowOffset[dir];
                const int nextCol = col + kColOffset[dir];

                if (!isInside(nextRow, nextCol) || maze_[nextRow][nextCol].visited)
                {
                    continue;
                }

                maze_[row][col].walls[dir] = false;
                maze_[nextRow][nextCol].walls[kOpposite[dir]] = false;
                generateByDFS(nextRow, nextCol);
            }
        }

        bool isInside(int row, int col) const
        {
            return row >= 0 && row < kMazeRows && col >= 0 && col < kMazeCols;
        }

        bool canMove(Direction dir) const
        {
            if (maze_[player_.row][player_.col].walls[dir])
            {
                return false;
            }

            const int nextRow = player_.row + kRowOffset[dir];
            const int nextCol = player_.col + kColOffset[dir];
            return isInside(nextRow, nextCol);
        }

        void handleInput()
        {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                running_ = false;
                return;
            }

            static bool rPressed = false;
            static bool hPressed = false;

            const bool nowRPressed = (GetAsyncKeyState('R') & 0x8000) != 0;
            if (nowRPressed && !rPressed)
            {
                resetGame();
            }
            rPressed = nowRPressed;

            const bool nowHPressed = (GetAsyncKeyState('H') & 0x8000) != 0;
            if (nowHPressed && !hPressed)
            {
                showHint_ = !showHint_;
                updateShortestPath();
            }
            hPressed = nowHPressed;

            if (gameWon_)
            {
                return;
            }

            const auto now = chrono::steady_clock::now();
            const auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastMoveTime_).count();
            if (elapsed < kMoveCooldownMs)
            {
                return;
            }

            Direction dir;
            bool hasMove = false;

            if (GetAsyncKeyState(VK_UP) & 0x8000)
            {
                dir = Up;
                hasMove = true;
            }
            else if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            {
                dir = Right;
                hasMove = true;
            }
            else if (GetAsyncKeyState(VK_DOWN) & 0x8000)
            {
                dir = Down;
                hasMove = true;
            }
            else if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            {
                dir = Left;
                hasMove = true;
            }

            if (!hasMove || !canMove(dir))
            {
                return;
            }

            player_.row += kRowOffset[dir];
            player_.col += kColOffset[dir];
            ++steps_;
            lastMoveTime_ = now;
            updateShortestPath();

            if (player_ == goalPoint_)
            {
                gameWon_ = true;
                winTime_ = chrono::steady_clock::now();
                const int totalSeconds = getElapsedSeconds();

                wstring message = L"Victory!\n\nTotal Steps: " + to_wstring(steps_)
                    + L"\nTotal Time: " + to_wstring(totalSeconds)
                    + L" s\n\nPress R to generate a new maze.";
                MessageBox(GetHWnd(), message.c_str(), _T("Maze Victory"), MB_OK | MB_ICONINFORMATION);
            }
        }

        int getElapsedSeconds() const
        {
            const auto endTime = gameWon_ ? winTime_ : chrono::steady_clock::now();
            return static_cast<int>(chrono::duration_cast<chrono::seconds>(endTime - startTime_).count());
        }

        wstring buildStatusText() const
        {
            return L"Steps: " + to_wstring(steps_) + L"   Time: " + to_wstring(getElapsedSeconds()) + L" s";
        }

        void updateShortestPath()
        {
            shortestPath_.clear();
            if (!showHint_)
            {
                return;
            }

            vector<vector<bool>> visited(kMazeRows, vector<bool>(kMazeCols, false));
            vector<vector<Point>> previous(kMazeRows, vector<Point>(kMazeCols, { -1, -1 }));
            queue<Point> q;

            q.push(player_);
            visited[player_.row][player_.col] = true;

            while (!q.empty())
            {
                Point current = q.front();
                q.pop();

                if (current == goalPoint_)
                {
                    break;
                }

                for (int dir = 0; dir < 4; ++dir)
                {
                    if (maze_[current.row][current.col].walls[dir])
                    {
                        continue;
                    }

                    const int nextRow = current.row + kRowOffset[dir];
                    const int nextCol = current.col + kColOffset[dir];

                    if (!isInside(nextRow, nextCol) || visited[nextRow][nextCol])
                    {
                        continue;
                    }

                    visited[nextRow][nextCol] = true;
                    previous[nextRow][nextCol] = current;
                    q.push({ nextRow, nextCol });
                }
            }

            if (!visited[goalPoint_.row][goalPoint_.col])
            {
                return;
            }

            for (Point cur = goalPoint_; !(cur == Point{ -1, -1 }); cur = previous[cur.row][cur.col])
            {
                shortestPath_.push_back(cur);
                if (cur == player_)
                {
                    break;
                }
            }

            reverse(shortestPath_.begin(), shortestPath_.end());
        }

        void draw()
        {
            cleardevice();
            drawInfoBar();
            drawMaze();
            FlushBatchDraw();
        }

        void drawInfoBar() const
        {
            setfillcolor(kInfoBgColor);
            solidrectangle(0, 0, kWindowWidth, kInfoBarHeight);

            setbkmode(TRANSPARENT);
            settextcolor(kTextColor);
            settextstyle(24, 0, _T("Consolas"));

            const wstring status = buildStatusText();
            outtextxy(16, 12, status.c_str());

            settextstyle(18, 0, _T("Consolas"));
            const wchar_t* hint = L"Arrow Keys: Move    H: Hint On/Off    R: Regenerate    ESC: Exit";
            outtextxy(16, 44, hint);
        }

        void drawMaze() const
        {
            setfillcolor(kFloorColor);
            solidrectangle(0, kInfoBarHeight, kWindowWidth, kWindowHeight);

            drawSpecialCell(startPoint_, kStartColor);
            drawSpecialCell(goalPoint_, kGoalColor);

            if (showHint_)
            {
                drawHintPath();
            }

            drawPlayer();
            drawWalls();
        }

        void drawSpecialCell(const Point& point, COLORREF color) const
        {
            const int left = point.col * kCellSize + 4;
            const int top = point.row * kCellSize + kInfoBarHeight + 4;
            const int right = left + kCellSize - 8;
            const int bottom = top + kCellSize - 8;

            setfillcolor(color);
            solidrectangle(left, top, right, bottom);
        }

        void drawHintPath() const
        {
            setfillcolor(kHintColor);

            for (const Point& point : shortestPath_)
            {
                if (point == player_ || point == goalPoint_)
                {
                    continue;
                }

                const int left = point.col * kCellSize + 7;
                const int top = point.row * kCellSize + kInfoBarHeight + 7;
                const int right = left + kCellSize - 14;
                const int bottom = top + kCellSize - 14;
                solidrectangle(left, top, right, bottom);
            }
        }

        void drawPlayer() const
        {
            const int left = player_.col * kCellSize + 5;
            const int top = player_.row * kCellSize + kInfoBarHeight + 5;
            const int right = left + kCellSize - 10;
            const int bottom = top + kCellSize - 10;

            setfillcolor(kPlayerColor);
            solidrectangle(left, top, right, bottom);
        }

        void drawWalls() const
        {
            setlinecolor(kWallColor);
            setlinestyle(PS_SOLID, kWallThickness);

            for (int row = 0; row < kMazeRows; ++row)
            {
                for (int col = 0; col < kMazeCols; ++col)
                {
                    const int x = col * kCellSize;
                    const int y = row * kCellSize + kInfoBarHeight;

                    const Cell& cell = maze_[row][col];
                    if (cell.walls[Up])
                    {
                        line(x, y, x + kCellSize, y);
                    }
                    if (cell.walls[Right])
                    {
                        line(x + kCellSize, y, x + kCellSize, y + kCellSize);
                    }
                    if (cell.walls[Down])
                    {
                        line(x, y + kCellSize, x + kCellSize, y + kCellSize);
                    }
                    if (cell.walls[Left])
                    {
                        line(x, y, x, y + kCellSize);
                    }
                }
            }
        }
    };
}

int main()
{
    MazeGame game;
    game.run();
    return 0;
}
