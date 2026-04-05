#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#include <windowsx.h>

using namespace std;

namespace
{
    constexpr int kInfoBarHeight = 92;
    constexpr int kWindowWidth = 1360;
    constexpr int kWindowHeight = 920;
    constexpr UINT_PTR kFrameTimerId = 1;
    constexpr int kMoveIntervalMs = 110;
    constexpr int kHintCost = 5;
    constexpr double kIsoTileHalfWidth = 26.0;
    constexpr double kIsoTileHalfHeight = 15.0;
    constexpr double kWallHeight = 30.0;

    constexpr COLORREF kBackgroundColor = RGB(206, 226, 243);
    constexpr COLORREF kPanelColor = RGB(241, 233, 214);
    constexpr COLORREF kPanelBorderColor = RGB(102, 92, 74);
    constexpr COLORREF kTextColor = RGB(42, 35, 24);
    constexpr COLORREF kWallTopColor = RGB(92, 92, 98);
    constexpr COLORREF kWallLeftColor = RGB(70, 70, 76);
    constexpr COLORREF kWallRightColor = RGB(58, 58, 63);
    constexpr COLORREF kFloorColor = RGB(234, 223, 197);
    constexpr COLORREF kFloorShadeColor = RGB(214, 203, 178);
    constexpr COLORREF kStartColor = RGB(87, 177, 98);
    constexpr COLORREF kHintColor = RGB(247, 214, 79);
    constexpr COLORREF kStarColor = RGB(255, 221, 87);
    constexpr COLORREF kButtonColor = RGB(72, 115, 171);
    constexpr COLORREF kButtonHoverColor = RGB(88, 136, 197);
    constexpr COLORREF kButtonTextColor = RGB(247, 244, 237);

    enum Direction
    {
        Up = 0,
        Right = 1,
        Down = 2,
        Left = 3,
        None = -1
    };

    enum class ScreenState
    {
        Menu,
        Playing,
        Victory
    };

    struct Cell
    {
        bool visited = false;
        bool walls[4] = { true, true, true, true };
        bool hasStar = false;
    };

    struct DifficultyOption
    {
        wstring label;
        int rows;
        int cols;
        RECT buttonRect{};
    };

    struct IsoPoint
    {
        LONG x = 0;
        LONG y = 0;
    };

    class MazeGame
    {
    public:
        MazeGame()
            : randomEngine_(static_cast<unsigned int>(time(nullptr)))
        {
            difficulties_.push_back({ L"LOW", 15, 15, {} });
            difficulties_.push_back({ L"MEDIUM", 23, 23, {} });
            difficulties_.push_back({ L"HIGH", 31, 31, {} });
            selectedDifficultyIndex_ = 1;
            currentRows_ = difficulties_[selectedDifficultyIndex_].rows;
            currentCols_ = difficulties_[selectedDifficultyIndex_].cols;
        }

        void Initialize(HWND hwnd)
        {
            hwnd_ = hwnd;
            UpdateMenuLayout();
            StartNewGame(selectedDifficultyIndex_);
            screenState_ = ScreenState::Menu;
        }

        void OnResize(int width, int height)
        {
            clientWidth_ = width;
            clientHeight_ = height;
            UpdateOrigin();
            UpdateMenuLayout();
        }

        void OnPaint(HDC hdc)
        {
            RECT clientRect{ 0, 0, clientWidth_, clientHeight_ };
            HBRUSH bgBrush = CreateSolidBrush(kBackgroundColor);
            FillRect(hdc, &clientRect, bgBrush);
            DeleteObject(bgBrush);

            DrawInfoPanel(hdc);

            if (screenState_ == ScreenState::Menu)
            {
                DrawMenu(hdc);
                return;
            }

            DrawMaze(hdc);
            DrawStars(hdc);
            DrawHint(hdc);
            DrawGoal(hdc);
            DrawPlayer(hdc);

            if (screenState_ == ScreenState::Victory)
            {
                DrawVictoryOverlay(hdc);
            }
        }

        void OnTimer()
        {
            if (screenState_ != ScreenState::Playing)
            {
                return;
            }

            const auto now = chrono::steady_clock::now();
            if (heldDirection_ != None &&
                chrono::duration_cast<chrono::milliseconds>(now - lastMoveTime_).count() >= kMoveIntervalMs)
            {
                TryMove(heldDirection_);
            }

            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void OnKeyDown(WPARAM key)
        {
            if (key == VK_ESCAPE)
            {
                PostMessage(hwnd_, WM_CLOSE, 0, 0);
                return;
            }

            if (screenState_ == ScreenState::Menu)
            {
                if (key == VK_RETURN || key == VK_SPACE)
                {
                    StartNewGame(selectedDifficultyIndex_);
                    screenState_ = ScreenState::Playing;
                    InvalidateRect(hwnd_, nullptr, TRUE);
                }
                return;
            }

            if (screenState_ == ScreenState::Victory)
            {
                if (key == 'R')
                {
                    StartNewGame(selectedDifficultyIndex_);
                    screenState_ = ScreenState::Playing;
                }
                InvalidateRect(hwnd_, nullptr, TRUE);
                return;
            }

            if (key == 'R')
            {
                StartNewGame(selectedDifficultyIndex_);
                InvalidateRect(hwnd_, nullptr, TRUE);
                return;
            }

            if (key == 'H')
            {
                showHintOverlay_ = !showHintOverlay_;
                InvalidateRect(hwnd_, nullptr, TRUE);
                return;
            }

            if (key == 'J')
            {
                TryPurchaseHint();
                InvalidateRect(hwnd_, nullptr, TRUE);
                return;
            }

            Direction direction = TranslateDirectionKey(key);
            if (direction != None)
            {
                heldDirection_ = direction;
                if (!keyHeld_[direction])
                {
                    keyHeld_[direction] = true;
                    TryMove(direction);
                }
                lastMoveTime_ = chrono::steady_clock::now();
            }
        }

        void OnKeyUp(WPARAM key)
        {
            Direction direction = TranslateDirectionKey(key);
            if (direction == None)
            {
                return;
            }

            keyHeld_[direction] = false;
            if (heldDirection_ == direction)
            {
                heldDirection_ = None;
                for (int candidate = 0; candidate < 4; ++candidate)
                {
                    if (keyHeld_[candidate])
                    {
                        heldDirection_ = static_cast<Direction>(candidate);
                    }
                }
            }
        }

        void OnLeftButtonDown(int x, int y)
        {
            if (screenState_ != ScreenState::Menu)
            {
                return;
            }

            POINT point{ x, y };
            for (size_t index = 0; index < difficulties_.size(); ++index)
            {
                if (PtInRect(&difficulties_[index].buttonRect, point))
                {
                    selectedDifficultyIndex_ = static_cast<int>(index);
                    StartNewGame(selectedDifficultyIndex_);
                    screenState_ = ScreenState::Playing;
                    InvalidateRect(hwnd_, nullptr, TRUE);
                    return;
                }
            }
        }

    private:
        HWND hwnd_ = nullptr;
        int clientWidth_ = kWindowWidth;
        int clientHeight_ = kWindowHeight;
        int currentRows_ = 23;
        int currentCols_ = 23;
        int selectedDifficultyIndex_ = 1;
        vector<DifficultyOption> difficulties_;
        vector<vector<Cell>> maze_;
        vector<POINT> activeHintCells_;
        ScreenState screenState_ = ScreenState::Menu;
        mt19937 randomEngine_;
        int playerRow_ = 0;
        int playerCol_ = 0;
        int steps_ = 0;
        int score_ = 0;
        int hintsPurchased_ = 0;
        bool showHintOverlay_ = false;
        Direction heldDirection_ = None;
        bool keyHeld_[4] = { false, false, false, false };
        chrono::steady_clock::time_point startTime_{};
        chrono::steady_clock::time_point lastMoveTime_{};
        double originX_ = kWindowWidth / 2.0;
        double originY_ = 170.0;

        void StartNewGame(int difficultyIndex)
        {
            selectedDifficultyIndex_ = difficultyIndex;
            currentRows_ = difficulties_[difficultyIndex].rows;
            currentCols_ = difficulties_[difficultyIndex].cols;
            maze_.assign(currentRows_, vector<Cell>(currentCols_));
            GenerateMaze();
            PlaceStars();
            playerRow_ = 0;
            playerCol_ = 0;
            steps_ = 0;
            score_ = 0;
            hintsPurchased_ = 0;
            showHintOverlay_ = false;
            heldDirection_ = None;
            fill(begin(keyHeld_), end(keyHeld_), false);
            activeHintCells_.clear();
            startTime_ = chrono::steady_clock::now();
            lastMoveTime_ = startTime_;
            UpdateOrigin();
        }

        void UpdateOrigin()
        {
            const double mazeWidth = (currentCols_ + currentRows_) * kIsoTileHalfWidth;
            originX_ = max(220.0, (clientWidth_ - mazeWidth) / 2.0 + currentRows_ * kIsoTileHalfWidth);
            originY_ = kInfoBarHeight + 90.0;
        }

        void UpdateMenuLayout()
        {
            const int buttonWidth = 210;
            const int buttonHeight = 78;
            const int gap = 28;
            const int totalWidth = buttonWidth * static_cast<int>(difficulties_.size()) + gap * 2;
            int startX = (clientWidth_ - totalWidth) / 2;
            const int top = clientHeight_ / 2;
            for (size_t index = 0; index < difficulties_.size(); ++index)
            {
                difficulties_[index].buttonRect.left = startX + static_cast<int>(index) * (buttonWidth + gap);
                difficulties_[index].buttonRect.top = top;
                difficulties_[index].buttonRect.right = difficulties_[index].buttonRect.left + buttonWidth;
                difficulties_[index].buttonRect.bottom = top + buttonHeight;
            }
        }

        Direction TranslateDirectionKey(WPARAM key) const
        {
            switch (key)
            {
            case VK_UP:
                return Up;
            case VK_RIGHT:
                return Right;
            case VK_DOWN:
                return Down;
            case VK_LEFT:
                return Left;
            default:
                return None;
            }
        }

        void GenerateMaze()
        {
            for (auto& row : maze_)
            {
                for (auto& cell : row)
                {
                    cell.visited = false;
                    cell.hasStar = false;
                    fill(begin(cell.walls), end(cell.walls), true);
                }
            }

            CarveFrom(0, 0);
            for (auto& row : maze_)
            {
                for (auto& cell : row)
                {
                    cell.visited = false;
                }
            }
        }

        void CarveFrom(int row, int col)
        {
            maze_[row][col].visited = true;
            array<int, 4> directions = { 0, 1, 2, 3 };
            shuffle(directions.begin(), directions.end(), randomEngine_);

            static const int dr[4] = { -1, 0, 1, 0 };
            static const int dc[4] = { 0, 1, 0, -1 };

            for (int direction : directions)
            {
                const int nextRow = row + dr[direction];
                const int nextCol = col + dc[direction];
                if (nextRow < 0 || nextRow >= currentRows_ || nextCol < 0 || nextCol >= currentCols_)
                {
                    continue;
                }
                if (maze_[nextRow][nextCol].visited)
                {
                    continue;
                }

                maze_[row][col].walls[direction] = false;
                maze_[nextRow][nextCol].walls[(direction + 2) % 4] = false;
                CarveFrom(nextRow, nextCol);
            }
        }
        void PlaceStars()
        {
            vector<POINT> cells;
            for (int row = 0; row < currentRows_; ++row)
            {
                for (int col = 0; col < currentCols_; ++col)
                {
                    if ((row == 0 && col == 0) || (row == currentRows_ - 1 && col == currentCols_ - 1))
                    {
                        continue;
                    }
                    cells.push_back({ col, row });
                }
            }

            shuffle(cells.begin(), cells.end(), randomEngine_);
            const int starCount = max(6, min(static_cast<int>(cells.size()) / 4, 28));
            for (int index = 0; index < starCount; ++index)
            {
                maze_[cells[index].y][cells[index].x].hasStar = true;
            }
        }

        bool CanMove(int row, int col, Direction direction) const
        {
            if (direction == None)
            {
                return false;
            }
            return !maze_[row][col].walls[direction];
        }

        void TryMove(Direction direction)
        {
            if (!CanMove(playerRow_, playerCol_, direction))
            {
                lastMoveTime_ = chrono::steady_clock::now();
                return;
            }

            switch (direction)
            {
            case Up:
                --playerRow_;
                break;
            case Right:
                ++playerCol_;
                break;
            case Down:
                ++playerRow_;
                break;
            case Left:
                --playerCol_;
                break;
            default:
                break;
            }

            ++steps_;
            lastMoveTime_ = chrono::steady_clock::now();
            CollectStarIfPresent();
            if (showHintOverlay_)
            {
                RebuildHint();
            }
            if (playerRow_ == currentRows_ - 1 && playerCol_ == currentCols_ - 1)
            {
                screenState_ = ScreenState::Victory;
                heldDirection_ = None;
                fill(begin(keyHeld_), end(keyHeld_), false);
                ShowVictoryMessage();
            }
        }

        void CollectStarIfPresent()
        {
            Cell& cell = maze_[playerRow_][playerCol_];
            if (cell.hasStar)
            {
                cell.hasStar = false;
                ++score_;
            }
        }

        void TryPurchaseHint()
        {
            if (score_ < kHintCost)
            {
                MessageBoxW(hwnd_, L"需要至少 5 分才能兑换一次通关提示。", L"提示不足", MB_OK | MB_ICONINFORMATION);
                return;
            }

            score_ -= kHintCost;
            ++hintsPurchased_;
            showHintOverlay_ = true;
            RebuildHint();
        }

        void RebuildHint()
        {
            vector<POINT> path = BuildShortestPath(playerRow_, playerCol_);
            activeHintCells_.clear();
            if (path.empty())
            {
                return;
            }

            const int revealCount = max(2, static_cast<int>(path.size()) / 3 + min(hintsPurchased_ * 2, 8));
            for (int index = 1; index < min(revealCount, static_cast<int>(path.size())); ++index)
            {
                activeHintCells_.push_back(path[index]);
            }
        }

        vector<POINT> BuildShortestPath(int startRow, int startCol) const
        {
            vector<vector<bool>> visited(currentRows_, vector<bool>(currentCols_, false));
            vector<vector<POINT>> parent(currentRows_, vector<POINT>(currentCols_, { -1, -1 }));
            vector<POINT> queue;
            queue.push_back({ startCol, startRow });
            visited[startRow][startCol] = true;

            static const int dr[4] = { -1, 0, 1, 0 };
            static const int dc[4] = { 0, 1, 0, -1 };

            size_t index = 0;
            while (index < queue.size())
            {
                POINT current = queue[index++];
                if (current.y == currentRows_ - 1 && current.x == currentCols_ - 1)
                {
                    break;
                }

                for (int direction = 0; direction < 4; ++direction)
                {
                    if (maze_[current.y][current.x].walls[direction])
                    {
                        continue;
                    }
                    const int nextRow = current.y + dr[direction];
                    const int nextCol = current.x + dc[direction];
                    if (nextRow < 0 || nextRow >= currentRows_ || nextCol < 0 || nextCol >= currentCols_)
                    {
                        continue;
                    }
                    if (visited[nextRow][nextCol])
                    {
                        continue;
                    }
                    visited[nextRow][nextCol] = true;
                    parent[nextRow][nextCol] = current;
                    queue.push_back({ nextCol, nextRow });
                }
            }

            if (!visited[currentRows_ - 1][currentCols_ - 1])
            {
                return {};
            }

            vector<POINT> path;
            POINT current{ currentCols_ - 1, currentRows_ - 1 };
            while (!(current.x == startCol && current.y == startRow))
            {
                path.push_back(current);
                current = parent[current.y][current.x];
            }
            path.push_back({ startCol, startRow });
            reverse(path.begin(), path.end());
            return path;
        }

        int ElapsedSeconds() const
        {
            return static_cast<int>(chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime_).count());
        }

        IsoPoint TileCenter(int row, int col) const
        {
            const double x = originX_ + (col - row) * kIsoTileHalfWidth;
            const double y = originY_ + (col + row) * kIsoTileHalfHeight;
            return { static_cast<LONG>(lround(x)), static_cast<LONG>(lround(y)) };
        }

        array<POINT, 4> TileDiamond(int row, int col, int inset = 0) const
        {
            const IsoPoint center = TileCenter(row, col);
            return {
                POINT{ center.x, static_cast<LONG>(center.y - kIsoTileHalfHeight + inset) },
                POINT{ static_cast<LONG>(center.x + kIsoTileHalfWidth - inset), center.y },
                POINT{ center.x, static_cast<LONG>(center.y + kIsoTileHalfHeight - inset) },
                POINT{ static_cast<LONG>(center.x - kIsoTileHalfWidth + inset), center.y }
            };
        }

        void DrawPolygon(HDC hdc, const POINT* points, int count, COLORREF fill, COLORREF border) const
        {
            HBRUSH brush = CreateSolidBrush(fill);
            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ oldBrush = SelectObject(hdc, brush);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            Polygon(hdc, points, count);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        }

        void FillCircle(HDC hdc, int centerX, int centerY, int radius, COLORREF color, COLORREF border) const
        {
            HBRUSH brush = CreateSolidBrush(color);
            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ oldBrush = SelectObject(hdc, brush);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            Ellipse(hdc, centerX - radius, centerY - radius, centerX + radius, centerY + radius);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        }
        void DrawInfoPanel(HDC hdc)
        {
            RECT panel{ 18, 14, clientWidth_ - 18, kInfoBarHeight };
            HBRUSH panelBrush = CreateSolidBrush(kPanelColor);
            HPEN panelPen = CreatePen(PS_SOLID, 2, kPanelBorderColor);
            HGDIOBJ oldBrush = SelectObject(hdc, panelBrush);
            HGDIOBJ oldPen = SelectObject(hdc, panelPen);
            RoundRect(hdc, panel.left, panel.top, panel.right, panel.bottom, 22, 22);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(panelBrush);
            DeleteObject(panelPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, kTextColor);
            HFONT titleFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            HFONT textFont = CreateFontW(20, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(hdc, titleFont);
            TextOutW(hdc, 40, 24, L"Maze Adventure 3D", 17);
            SelectObject(hdc, textFont);

            wstringstream status;
            status << L"Difficulty: " << difficulties_[selectedDifficultyIndex_].label
                   << L"   Steps: " << steps_
                   << L"   Time: " << ElapsedSeconds() << L"s"
                   << L"   Score: " << score_
                   << L"   Hints: " << hintsPurchased_;
            const wstring statusText = status.str();
            TextOutW(hdc, 40, 56, statusText.c_str(), static_cast<int>(statusText.size()));

            const wstring controls =
                screenState_ == ScreenState::Menu
                ? L"Choose a difficulty to start."
                : L"Arrows: move continuously  H: show/hide hint  J: spend 5 score for path  R: restart  ESC: quit";
            TextOutW(hdc, 520, 56, controls.c_str(), static_cast<int>(controls.size()));

            SelectObject(hdc, oldFont);
            DeleteObject(titleFont);
            DeleteObject(textFont);
        }

        void DrawMenu(HDC hdc)
        {
            HFONT titleFont = CreateFontW(58, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Georgia");
            HFONT subFont = CreateFontW(24, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            HFONT buttonFont = CreateFontW(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(37, 44, 52));

            HGDIOBJ oldFont = SelectObject(hdc, titleFont);
            const wstring title = L"Maze Adventure";
            TextOutW(hdc, clientWidth_ / 2 - 210, 180, title.c_str(), static_cast<int>(title.size()));
            SelectObject(hdc, subFont);
            const wstring subtitle = L"Choose a difficulty and enter a pseudo-3D maze full of stars, hints and a modeled hero.";
            TextOutW(hdc, clientWidth_ / 2 - 420, 250, subtitle.c_str(), static_cast<int>(subtitle.size()));

            RECT preview{ clientWidth_ / 2 - 200, 320, clientWidth_ / 2 + 200, 470 };
            HBRUSH previewBrush = CreateSolidBrush(RGB(227, 217, 189));
            HPEN previewPen = CreatePen(PS_SOLID, 2, RGB(115, 104, 85));
            HGDIOBJ oldBrush = SelectObject(hdc, previewBrush);
            HGDIOBJ oldPen = SelectObject(hdc, previewPen);
            RoundRect(hdc, preview.left, preview.top, preview.right, preview.bottom, 26, 26);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(previewBrush);
            DeleteObject(previewPen);
            DrawMiniPreview(hdc, preview);

            SelectObject(hdc, buttonFont);
            for (size_t index = 0; index < difficulties_.size(); ++index)
            {
                const RECT& button = difficulties_[index].buttonRect;
                const bool selected = static_cast<int>(index) == selectedDifficultyIndex_;
                HBRUSH brush = CreateSolidBrush(selected ? kButtonHoverColor : kButtonColor);
                HPEN pen = CreatePen(PS_SOLID, 2, RGB(34, 61, 98));
                oldBrush = SelectObject(hdc, brush);
                oldPen = SelectObject(hdc, pen);
                RoundRect(hdc, button.left, button.top, button.right, button.bottom, 24, 24);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(brush);
                DeleteObject(pen);
                SetTextColor(hdc, kButtonTextColor);
                DrawCenteredText(hdc, button, difficulties_[index].label);
            }

            SelectObject(hdc, subFont);
            SetTextColor(hdc, RGB(60, 63, 68));
            const wstring footer = L"LOW 15x15   MEDIUM 23x23   HIGH 31x31";
            TextOutW(hdc, clientWidth_ / 2 - 190, clientHeight_ / 2 + 100, footer.c_str(), static_cast<int>(footer.size()));

            SelectObject(hdc, oldFont);
            DeleteObject(titleFont);
            DeleteObject(subFont);
            DeleteObject(buttonFont);
        }

        void DrawMiniPreview(HDC hdc, const RECT& bounds)
        {
            POINT diamond[4] = {
                { bounds.left + 80, bounds.top + 55 },
                { bounds.left + 145, bounds.top + 85 },
                { bounds.left + 80, bounds.top + 115 },
                { bounds.left + 15, bounds.top + 85 }
            };
            DrawPolygon(hdc, diamond, 4, kFloorColor, RGB(110, 104, 90));

            POINT heroBody[4] = {
                { bounds.left + 263, bounds.top + 58 },
                { bounds.left + 278, bounds.top + 88 },
                { bounds.left + 263, bounds.top + 118 },
                { bounds.left + 248, bounds.top + 88 }
            };
            DrawPolygon(hdc, heroBody, 4, RGB(73, 123, 208), RGB(40, 72, 120));
            FillCircle(hdc, bounds.left + 263, bounds.top + 48, 10, RGB(245, 216, 189), RGB(120, 93, 79));

            POINT doorLeft[4] = {
                { bounds.right - 95, bounds.top + 38 },
                { bounds.right - 65, bounds.top + 54 },
                { bounds.right - 65, bounds.top + 112 },
                { bounds.right - 95, bounds.top + 96 }
            };
            POINT doorRight[4] = {
                { bounds.right - 65, bounds.top + 54 },
                { bounds.right - 43, bounds.top + 43 },
                { bounds.right - 43, bounds.top + 101 },
                { bounds.right - 65, bounds.top + 112 }
            };
            DrawPolygon(hdc, doorLeft, 4, RGB(155, 77, 57), RGB(95, 45, 33));
            DrawPolygon(hdc, doorRight, 4, RGB(188, 96, 70), RGB(95, 45, 33));

            POINT star[10];
            const int centerX = bounds.left + 178;
            const int centerY = bounds.top + 86;
            for (int i = 0; i < 10; ++i)
            {
                const double angle = -3.14159265 / 2.0 + i * 3.14159265 / 5.0;
                const int radius = (i % 2 == 0) ? 18 : 8;
                star[i].x = static_cast<LONG>(lround(centerX + cos(angle) * radius));
                star[i].y = static_cast<LONG>(lround(centerY + sin(angle) * radius));
            }
            DrawPolygon(hdc, star, 10, kStarColor, RGB(170, 125, 33));
        }

        void DrawCenteredText(HDC hdc, const RECT& rect, const wstring& text) const
        {
            RECT local = rect;
            DrawTextW(hdc, text.c_str(), static_cast<int>(text.size()), &local, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        void DrawMaze(HDC hdc)
        {
            for (int sum = 0; sum <= currentRows_ + currentCols_ - 2; ++sum)
            {
                for (int row = 0; row < currentRows_; ++row)
                {
                    const int col = sum - row;
                    if (col < 0 || col >= currentCols_)
                    {
                        continue;
                    }
                    DrawCell(hdc, row, col);
                }
            }
        }

        void DrawCell(HDC hdc, int row, int col)
        {
            auto diamond = TileDiamond(row, col);
            COLORREF fillColor = kFloorColor;
            if (row == 0 && col == 0)
            {
                fillColor = kStartColor;
            }
            else if (row == currentRows_ - 1 && col == currentCols_ - 1)
            {
                fillColor = RGB(226, 214, 190);
            }
            DrawPolygon(hdc, diamond.data(), 4, fillColor, kFloorShadeColor);

            if (maze_[row][col].walls[Left])
            {
                const POINT wall[4] = {
                    diamond[3],
                    diamond[0],
                    { diamond[0].x, static_cast<LONG>(diamond[0].y + kWallHeight) },
                    { diamond[3].x, static_cast<LONG>(diamond[3].y + kWallHeight) }
                };
                DrawPolygon(hdc, wall, 4, kWallLeftColor, RGB(46, 46, 50));
            }
            if (maze_[row][col].walls[Up])
            {
                const POINT wall[4] = {
                    diamond[0],
                    diamond[1],
                    { diamond[1].x, static_cast<LONG>(diamond[1].y + kWallHeight) },
                    { diamond[0].x, static_cast<LONG>(diamond[0].y + kWallHeight) }
                };
                DrawPolygon(hdc, wall, 4, kWallTopColor, RGB(56, 56, 60));
            }
            if (maze_[row][col].walls[Right])
            {
                const POINT wall[4] = {
                    diamond[1],
                    diamond[2],
                    { diamond[2].x, static_cast<LONG>(diamond[2].y + kWallHeight) },
                    { diamond[1].x, static_cast<LONG>(diamond[1].y + kWallHeight) }
                };
                DrawPolygon(hdc, wall, 4, kWallRightColor, RGB(46, 46, 50));
            }
            if (maze_[row][col].walls[Down])
            {
                const POINT wall[4] = {
                    diamond[2],
                    diamond[3],
                    { diamond[3].x, static_cast<LONG>(diamond[3].y + kWallHeight) },
                    { diamond[2].x, static_cast<LONG>(diamond[2].y + kWallHeight) }
                };
                DrawPolygon(hdc, wall, 4, RGB(77, 77, 82), RGB(46, 46, 50));
            }
        }

        void DrawStars(HDC hdc)
        {
            for (int row = 0; row < currentRows_; ++row)
            {
                for (int col = 0; col < currentCols_; ++col)
                {
                    if (!maze_[row][col].hasStar)
                    {
                        continue;
                    }
                    DrawStar(hdc, row, col);
                }
            }
        }

        void DrawStar(HDC hdc, int row, int col)
        {
            const IsoPoint center = TileCenter(row, col);
            POINT points[10];
            for (int i = 0; i < 10; ++i)
            {
                const double angle = -3.14159265 / 2.0 + i * 3.14159265 / 5.0;
                const int radius = (i % 2 == 0) ? 11 : 5;
                points[i].x = static_cast<LONG>(lround(center.x + cos(angle) * radius));
                points[i].y = static_cast<LONG>(lround(center.y - 6 + sin(angle) * radius));
            }
            DrawPolygon(hdc, points, 10, kStarColor, RGB(171, 122, 30));
        }

        void DrawHint(HDC hdc)
        {
            if (!showHintOverlay_ || activeHintCells_.empty())
            {
                return;
            }

            for (const POINT& cell : activeHintCells_)
            {
                auto diamond = TileDiamond(cell.y, cell.x, 6);
                DrawPolygon(hdc, diamond.data(), 4, kHintColor, RGB(180, 148, 45));
            }
        }

        void DrawPlayer(HDC hdc)
        {
            const IsoPoint center = TileCenter(playerRow_, playerCol_);
            POINT shadow[4] = {
                { center.x, center.y + 4 },
                { center.x + 14, center.y + 12 },
                { center.x, center.y + 18 },
                { center.x - 14, center.y + 12 }
            };
            DrawPolygon(hdc, shadow, 4, RGB(110, 102, 92), RGB(110, 102, 92));

            FillCircle(hdc, center.x, center.y - 24, 10, RGB(247, 219, 193), RGB(124, 98, 79));

            POINT torso[4] = {
                { center.x, center.y - 16 },
                { center.x + 14, center.y + 8 },
                { center.x, center.y + 20 },
                { center.x - 14, center.y + 8 }
            };
            DrawPolygon(hdc, torso, 4, RGB(68, 116, 211), RGB(39, 67, 126));

            HPEN limbPen = CreatePen(PS_SOLID, 4, RGB(54, 61, 76));
            HGDIOBJ oldPen = SelectObject(hdc, limbPen);
            MoveToEx(hdc, center.x - 7, center.y + 12, nullptr);
            LineTo(hdc, center.x - 13, center.y + 28);
            MoveToEx(hdc, center.x + 7, center.y + 12, nullptr);
            LineTo(hdc, center.x + 13, center.y + 28);
            MoveToEx(hdc, center.x - 8, center.y - 2, nullptr);
            LineTo(hdc, center.x - 17, center.y + 10);
            MoveToEx(hdc, center.x + 8, center.y - 2, nullptr);
            LineTo(hdc, center.x + 17, center.y + 10);
            SelectObject(hdc, oldPen);
            DeleteObject(limbPen);
        }

        void DrawGoal(HDC hdc)
        {
            const IsoPoint center = TileCenter(currentRows_ - 1, currentCols_ - 1);
            POINT leftFace[4] = {
                { center.x - 20, center.y - 32 },
                { center.x, center.y - 44 },
                { center.x, center.y + 10 },
                { center.x - 20, center.y + 22 }
            };
            POINT rightFace[4] = {
                { center.x, center.y - 44 },
                { center.x + 20, center.y - 32 },
                { center.x + 20, center.y + 22 },
                { center.x, center.y + 10 }
            };
            DrawPolygon(hdc, leftFace, 4, RGB(149, 72, 52), RGB(84, 42, 29));
            DrawPolygon(hdc, rightFace, 4, RGB(189, 96, 70), RGB(84, 42, 29));
            FillCircle(hdc, center.x, center.y - 44, 15, RGB(173, 83, 61), RGB(84, 42, 29));
            FillCircle(hdc, center.x + 8, center.y - 8, 3, RGB(228, 192, 94), RGB(108, 84, 39));
        }

        void DrawVictoryOverlay(HDC hdc)
        {
            RECT overlay{ clientWidth_ / 2 - 260, clientHeight_ / 2 - 110, clientWidth_ / 2 + 260, clientHeight_ / 2 + 110 };
            HBRUSH brush = CreateSolidBrush(RGB(250, 244, 228));
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(122, 104, 66));
            HGDIOBJ oldBrush = SelectObject(hdc, brush);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            RoundRect(hdc, overlay.left, overlay.top, overlay.right, overlay.bottom, 28, 28);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);

            HFONT titleFont = CreateFontW(34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            HFONT textFont = CreateFontW(22, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, kTextColor);
            HGDIOBJ oldFont = SelectObject(hdc, titleFont);
            const wstring title = L"Victory!";
            TextOutW(hdc, overlay.left + 190, overlay.top + 22, title.c_str(), static_cast<int>(title.size()));
            SelectObject(hdc, textFont);
            wstringstream summary;
            summary << L"Steps: " << steps_ << L"    Time: " << ElapsedSeconds() << L"s    Score: " << score_;
            const wstring summaryText = summary.str();
            TextOutW(hdc, overlay.left + 84, overlay.top + 88, summaryText.c_str(), static_cast<int>(summaryText.size()));
            const wstring footer = L"Press R to generate a new maze.";
            TextOutW(hdc, overlay.left + 112, overlay.top + 136, footer.c_str(), static_cast<int>(footer.size()));
            SelectObject(hdc, oldFont);
            DeleteObject(titleFont);
            DeleteObject(textFont);
        }

        void ShowVictoryMessage() const
        {
            wstringstream message;
            message << L"你成功走出了迷宫！\n\n总步数: " << steps_
                    << L"\n总时间: " << ElapsedSeconds()
                    << L" 秒\n当前分数: " << score_;
            MessageBoxW(hwnd_, message.str().c_str(), L"Victory", MB_OK | MB_ICONINFORMATION);
        }
    };

    MazeGame g_game;

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            g_game.Initialize(hwnd);
            SetTimer(hwnd, kFrameTimerId, 16, nullptr);
            return 0;
        case WM_SIZE:
            g_game.OnResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_TIMER:
            if (wParam == kFrameTimerId)
            {
                g_game.OnTimer();
            }
            return 0;
        case WM_KEYDOWN:
            g_game.OnKeyDown(wParam);
            return 0;
        case WM_KEYUP:
            g_game.OnKeyUp(wParam);
            return 0;
        case WM_LBUTTONDOWN:
            g_game.OnLeftButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            g_game.OnPaint(hdc);
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
    const wchar_t kClassName[] = L"MazeAdventure3DWindow";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));

    if (!RegisterClassW(&windowClass))
    {
        MessageBoxW(nullptr, L"Window registration failed.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    RECT desiredRect{ 0, 0, kWindowWidth, kWindowHeight };
    AdjustWindowRect(&desiredRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"Maze Adventure 3D",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        desiredRect.right - desiredRect.left,
        desiredRect.bottom - desiredRect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr)
    {
        MessageBoxW(nullptr, L"Window creation failed.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
