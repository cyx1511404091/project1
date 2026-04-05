#include <algorithm>
#include <chrono>
#include <ctime>
#include <random>
#include <string>
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
    constexpr int kWindowWidth = kMazeCols * kCellSize;
    constexpr int kWindowHeight = kMazeRows * kCellSize + kInfoBarHeight;
    constexpr UINT_PTR kFrameTimerId = 1;

    constexpr COLORREF kWallColor = RGB(64, 64, 64);
    constexpr COLORREF kFloorColor = RGB(241, 232, 208);
    constexpr COLORREF kPlayerColor = RGB(40, 110, 255);
    constexpr COLORREF kStartColor = RGB(68, 176, 86);
    constexpr COLORREF kGoalColor = RGB(215, 58, 73);
    constexpr COLORREF kHintColor = RGB(245, 208, 66);
    constexpr COLORREF kTextColor = RGB(32, 32, 32);
    constexpr COLORREF kInfoBgColor = RGB(225, 217, 193);

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

        void setWindow(HWND hwnd)
        {
            hwnd_ = hwnd;
        }

        void resetGame()
        {
            for (auto& row : maze_)
            {
                for (auto& cell : row)
                {
                    cell = Cell();
                }
            }

            generateByDFS(startPoint_.row, startPoint_.col);
            player_ = startPoint_;
            steps_ = 0;
            showHint_ = false;
            gameWon_ = false;
            startTime_ = chrono::steady_clock::now();
            winTime_ = startTime_;
            updateShortestPath();
        }

        void toggleHint()
        {
            showHint_ = !showHint_;
            updateShortestPath();
        }

        void tryMove(Direction dir)
        {
            if (gameWon_ || maze_[player_.row][player_.col].walls[dir])
            {
                return;
            }

            const int nextRow = player_.row + kRowOffset[dir];
            const int nextCol = player_.col + kColOffset[dir];
            if (nextRow < 0 || nextRow >= kMazeRows || nextCol < 0 || nextCol >= kMazeCols)
            {
                return;
            }

            player_.row = nextRow;
            player_.col = nextCol;
            ++steps_;
            updateShortestPath();

            if (player_ == goalPoint_)
            {
                gameWon_ = true;
                winTime_ = chrono::steady_clock::now();
                const wstring message = L"Victory!\n\nTotal Steps: " + to_wstring(steps_)
                    + L"\nTotal Time: " + to_wstring(getElapsedSeconds())
                    + L" s\n\nPress R to generate a new maze.";
                MessageBoxW(hwnd_, message.c_str(), L"Maze Victory", MB_OK | MB_ICONINFORMATION);
            }
        }

        void draw(HDC targetDc) const
        {
            HBRUSH floorBrush = CreateSolidBrush(kFloorColor);
            RECT clientRect{ 0, 0, kWindowWidth, kWindowHeight };
            FillRect(targetDc, &clientRect, floorBrush);
            DeleteObject(floorBrush);

            HBRUSH infoBrush = CreateSolidBrush(kInfoBgColor);
            RECT infoRect{ 0, 0, kWindowWidth, kInfoBarHeight };
            FillRect(targetDc, &infoRect, infoBrush);
            DeleteObject(infoBrush);

            SetBkMode(targetDc, TRANSPARENT);
            SetTextColor(targetDc, kTextColor);

            HFONT titleFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
            HFONT bodyFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
            HFONT oldFont = static_cast<HFONT>(SelectObject(targetDc, titleFont));

            const wstring status = L"Steps: " + to_wstring(steps_) + L"   Time: " + to_wstring(getElapsedSeconds()) + L" s";
            TextOutW(targetDc, 16, 12, status.c_str(), static_cast<int>(status.size()));

            SelectObject(targetDc, bodyFont);
            const wchar_t* hintText = L"Arrow Keys: Move    H: Hint On/Off    R: Regenerate    ESC: Exit";
            TextOutW(targetDc, 16, 44, hintText, lstrlenW(hintText));

            drawCells(targetDc);
            drawWalls(targetDc);

            SelectObject(targetDc, oldFont);
            DeleteObject(titleFont);
            DeleteObject(bodyFont);
        }

    private:
        vector<vector<Cell>> maze_;
        mt19937 rng_;
        Point player_;
        Point startPoint_;
        Point goalPoint_;
        vector<Point> shortestPath_;
        HWND hwnd_ = nullptr;

        bool showHint_ = false;
        bool gameWon_ = false;
        int steps_ = 0;
        chrono::steady_clock::time_point startTime_;
        chrono::steady_clock::time_point winTime_;

        int getElapsedSeconds() const
        {
            const auto endTime = gameWon_ ? winTime_ : chrono::steady_clock::now();
            return static_cast<int>(chrono::duration_cast<chrono::seconds>(endTime - startTime_).count());
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
                if (nextRow < 0 || nextRow >= kMazeRows || nextCol < 0 || nextCol >= kMazeCols || maze_[nextRow][nextCol].visited)
                {
                    continue;
                }

                maze_[row][col].walls[dir] = false;
                maze_[nextRow][nextCol].walls[kOpposite[dir]] = false;
                generateByDFS(nextRow, nextCol);
            }
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
            vector<Point> bfsQueue;
            bfsQueue.push_back(player_);
            visited[player_.row][player_.col] = true;

            for (size_t head = 0; head < bfsQueue.size(); ++head)
            {
                const Point current = bfsQueue[head];
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
                    if (nextRow < 0 || nextRow >= kMazeRows || nextCol < 0 || nextCol >= kMazeCols || visited[nextRow][nextCol])
                    {
                        continue;
                    }

                    visited[nextRow][nextCol] = true;
                    previous[nextRow][nextCol] = current;
                    bfsQueue.push_back({ nextRow, nextCol });
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

        void fillCell(HDC dc, const Point& point, COLORREF color, int margin) const
        {
            RECT rect{
                point.col * kCellSize + margin,
                point.row * kCellSize + kInfoBarHeight + margin,
                (point.col + 1) * kCellSize - margin,
                (point.row + 1) * kCellSize + kInfoBarHeight - margin
            };
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        }

        void drawCells(HDC dc) const
        {
            fillCell(dc, startPoint_, kStartColor, 4);
            fillCell(dc, goalPoint_, kGoalColor, 4);

            if (showHint_)
            {
                for (const Point& point : shortestPath_)
                {
                    if (point == player_ || point == goalPoint_)
                    {
                        continue;
                    }
                    fillCell(dc, point, kHintColor, 7);
                }
            }

            fillCell(dc, player_, kPlayerColor, 5);
        }

        void drawWalls(HDC dc) const
        {
            HPEN pen = CreatePen(PS_SOLID, kWallThickness, kWallColor);
            HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));

            for (int row = 0; row < kMazeRows; ++row)
            {
                for (int col = 0; col < kMazeCols; ++col)
                {
                    const int x = col * kCellSize;
                    const int y = row * kCellSize + kInfoBarHeight;
                    const Cell& cell = maze_[row][col];

                    if (cell.walls[Up])
                    {
                        MoveToEx(dc, x, y, nullptr);
                        LineTo(dc, x + kCellSize, y);
                    }
                    if (cell.walls[Right])
                    {
                        MoveToEx(dc, x + kCellSize, y, nullptr);
                        LineTo(dc, x + kCellSize, y + kCellSize);
                    }
                    if (cell.walls[Down])
                    {
                        MoveToEx(dc, x, y + kCellSize, nullptr);
                        LineTo(dc, x + kCellSize, y + kCellSize);
                    }
                    if (cell.walls[Left])
                    {
                        MoveToEx(dc, x, y, nullptr);
                        LineTo(dc, x, y + kCellSize);
                    }
                }
            }

            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }
    };

    MazeGame g_game;

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            g_game.setWindow(hwnd);
            SetTimer(hwnd, kFrameTimerId, 16, nullptr);
            return 0;

        case WM_TIMER:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_KEYDOWN:
            switch (wParam)
            {
            case VK_UP:
                g_game.tryMove(Up);
                break;
            case VK_RIGHT:
                g_game.tryMove(Right);
                break;
            case VK_DOWN:
                g_game.tryMove(Down);
                break;
            case VK_LEFT:
                g_game.tryMove(Left);
                break;
            case 'H':
                g_game.toggleHint();
                break;
            case 'R':
                g_game.resetGame();
                break;
            case VK_ESCAPE:
                DestroyWindow(hwnd);
                break;
            default:
                break;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC hdc = BeginPaint(hwnd, &ps);
                HDC memDc = CreateCompatibleDC(hdc);
                HBITMAP bitmap = CreateCompatibleBitmap(hdc, kWindowWidth, kWindowHeight);
                HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDc, bitmap));

                g_game.draw(memDc);
                BitBlt(hdc, 0, 0, kWindowWidth, kWindowHeight, memDc, 0, 0, SRCCOPY);

                SelectObject(memDc, oldBitmap);
                DeleteObject(bitmap);
                DeleteDC(memDc);
                EndPaint(hwnd, &ps);
                return 0;
            }

        case WM_DESTROY:
            KillTimer(hwnd, kFrameTimerId);
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    const wchar_t kClassName[] = L"MazeGameWindowClass";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc);

    RECT rect{ 0, 0, kWindowWidth, kWindowHeight };
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"Maze Game",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (hwnd == nullptr)
    {
        return 0;
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
