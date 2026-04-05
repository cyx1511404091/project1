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

    struct Rect
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    enum class Scene
    {
        StartMenu,
        Playing
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
        Rect startButton_{ kWindowWidth / 2 - 120, kWindowHeight / 2 + 20, kWindowWidth / 2 + 120, kWindowHeight / 2 + 92 };

        Scene scene_ = Scene::StartMenu;
        bool running_ = true;
        bool showHint_ = false;
        bool gameWon_ = false;
        bool leftMousePressed_ = false;
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

        void startGame()
        {
            resetGame();
            scene_ = Scene::Playing;
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

        POINT getMousePosition() const
        {
            POINT cursorPos{};
            GetCursorPos(&cursorPos);
            ScreenToClient(GetHWnd(), &cursorPos);
            return cursorPos;
        }

        bool isInsideRect(int x, int y, const Rect& rect) const
        {
            return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
        }

        void handleInput()
        {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                running_ = false;
                return;
            }

            if (scene_ == Scene::StartMenu)
            {
                handleStartMenuInput();
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

        void handleStartMenuInput()
        {
            const bool enterPressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
            const bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (enterPressed || spacePressed)
            {
                startGame();
                return;
            }

            const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (mouseDown && !leftMousePressed_)
            {
                const POINT mousePos = getMousePosition();
                if (isInsideRect(mousePos.x, mousePos.y, startButton_))
                {
                    startGame();
                }
            }

            leftMousePressed_ = mouseDown;
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
            if (scene_ == Scene::StartMenu)
            {
                drawStartMenu();
            }
            else
            {
                drawInfoBar();
                drawMaze();
            }
            FlushBatchDraw();
        }

        void drawStartMenu() const
        {
            setfillcolor(kFloorColor);
            solidrectangle(0, 0, kWindowWidth, kWindowHeight);

            setlinecolor(kWallColor);
            setlinestyle(PS_SOLID, 3);
            rectangle(20, 20, kWindowWidth - 20, kWindowHeight - 20);

            setbkmode(TRANSPARENT);
            settextcolor(kWallColor);
            settextstyle(46, 0, _T("Consolas"));
            outtextxy(kWindowWidth / 2 - 150, 90, _T("MAZE QUEST"));

            settextstyle(22, 0, _T("Consolas"));
            outtextxy(88, 170, _T("DFS maze generation  |  BFS shortest hint  |  Smooth movement"));
            outtextxy(118, 210, _T("Arrow Keys: Move    H: Hint    R: Regenerate    ESC: Exit"));

            const POINT mousePos = getMousePosition();
            const bool hovering = isInsideRect(mousePos.x, mousePos.y, startButton_);
            const COLORREF buttonColor = hovering ? RGB(50, 135, 255) : kPlayerColor;

            setfillcolor(buttonColor);
            solidroundrect(startButton_.left, startButton_.top, startButton_.right, startButton_.bottom, 18, 18);

            drawStartButtonIcon();

            settextcolor(WHITE);
            settextstyle(28, 0, _T("Consolas"));
            outtextxy(startButton_.left + 88, startButton_.top + 22, _T("START GAME"));

            settextcolor(kTextColor);
            settextstyle(18, 0, _T("Consolas"));
            outtextxy(kWindowWidth / 2 - 105, startButton_.bottom + 30, _T("Click the button or press Enter"));
        }

        void drawStartButtonIcon() const
        {
            const int iconLeft = startButton_.left + 22;
            const int iconTop = startButton_.top + 14;
            const int iconRight = iconLeft + 44;
            const int iconBottom = iconTop + 44;

            setlinecolor(WHITE);
            setlinestyle(PS_SOLID, 2);
            rectangle(iconLeft, iconTop, iconRight, iconBottom);

            setlinecolor(kHintColor);
            line(iconLeft + 8, iconTop + 10, iconLeft + 24, iconTop + 10);
            line(iconLeft + 24, iconTop + 10, iconLeft + 24, iconTop + 20);
            line(iconLeft + 24, iconTop + 20, iconLeft + 34, iconTop + 20);
            line(iconLeft + 34, iconTop + 20, iconLeft + 34, iconTop + 32);

            setfillcolor(WHITE);
            POINT triangle[3] = {
                { iconLeft + 14, iconTop + 25 },
                { iconLeft + 14, iconTop + 37 },
                { iconLeft + 28, iconTop + 31 }
            };
            solidpolygon(triangle, 3);
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
