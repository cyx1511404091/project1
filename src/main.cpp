#include <algorithm>
#include <array>
#include <chrono>
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
    constexpr int kMoveIntervalMs = 130;
    constexpr int kTurnIntervalMs = 150;
    constexpr int kHintCost = 5;

    constexpr COLORREF kSkyColor = RGB(192, 219, 244);
    constexpr COLORREF kFloorColor = RGB(140, 120, 92);
    constexpr COLORREF kPanelColor = RGB(241, 233, 214);
    constexpr COLORREF kPanelBorderColor = RGB(102, 92, 74);
    constexpr COLORREF kTextColor = RGB(42, 35, 24);
    constexpr COLORREF kWallNearColor = RGB(104, 104, 112);
    constexpr COLORREF kWallMidColor = RGB(88, 88, 97);
    constexpr COLORREF kWallFarColor = RGB(72, 72, 81);
    constexpr COLORREF kDoorColor = RGB(176, 92, 62);
    constexpr COLORREF kDoorDarkColor = RGB(111, 55, 36);
    constexpr COLORREF kHintColor = RGB(247, 214, 79);
    constexpr COLORREF kStarColor = RGB(255, 221, 87);
    constexpr COLORREF kButtonColor = RGB(72, 115, 171);
    constexpr COLORREF kButtonHoverColor = RGB(88, 136, 197);
    constexpr COLORREF kButtonTextColor = RGB(247, 244, 237);
    constexpr COLORREF kArmColor = RGB(235, 204, 178);
    constexpr COLORREF kSleeveColor = RGB(64, 115, 205);
    constexpr COLORREF kMiniPathColor = RGB(228, 220, 198);
    constexpr COLORREF kMiniPlayerColor = RGB(46, 110, 236);
    constexpr COLORREF kMiniGoalColor = RGB(196, 66, 52);
    constexpr COLORREF kMiniStarColor = RGB(240, 199, 55);

    enum Direction
    {
        North = 0,
        East = 1,
        South = 2,
        West = 3,
        None = -1
    };

    enum class ScreenState
    {
        Menu,
        Playing,
        Victory
    };

    enum class ActionKey
    {
        None,
        Forward,
        Backward,
        TurnLeft,
        TurnRight
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

    struct CorridorStep
    {
        bool leftWall = true;
        bool rightWall = true;
        bool frontWall = true;
        bool hasStar = false;
        bool isGoal = false;
        bool isHint = false;
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
            UpdateMenuLayout();
        }

        void OnPaint(HDC hdc)
        {
            RECT clientRect{ 0, 0, clientWidth_, clientHeight_ };
            HDC memoryDc = CreateCompatibleDC(hdc);
            HBITMAP bitmap = CreateCompatibleBitmap(hdc, clientWidth_, clientHeight_);
            HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

            HBRUSH bgBrush = CreateSolidBrush(kSkyColor);
            FillRect(memoryDc, &clientRect, bgBrush);
            DeleteObject(bgBrush);

            DrawInfoPanel(memoryDc);

            if (screenState_ == ScreenState::Menu)
            {
                DrawMenu(memoryDc);
            }
            else
            {
                DrawFirstPersonView(memoryDc);
                DrawMiniMap(memoryDc);
                DrawPlayerHands(memoryDc);

                if (screenState_ == ScreenState::Victory)
                {
                    DrawVictoryOverlay(memoryDc);
                }
            }

            BitBlt(hdc, 0, 0, clientWidth_, clientHeight_, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
        }

        void OnTimer()
        {
            if (screenState_ != ScreenState::Playing)
            {
                return;
            }

            const auto now = chrono::steady_clock::now();
            if (heldAction_ != ActionKey::None)
            {
                const int interval = (heldAction_ == ActionKey::TurnLeft || heldAction_ == ActionKey::TurnRight)
                    ? kTurnIntervalMs
                    : kMoveIntervalMs;
                if (chrono::duration_cast<chrono::milliseconds>(now - lastActionTime_).count() >= interval)
                {
                    PerformAction(heldAction_);
                }
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

            const ActionKey action = TranslateActionKey(key);
            if (action != ActionKey::None)
            {
                heldAction_ = action;
                if (!IsActionHeld(action))
                {
                    SetActionHeld(action, true);
                    PerformAction(action);
                }
                lastActionTime_ = chrono::steady_clock::now();
            }
        }

        void OnKeyUp(WPARAM key)
        {
            const ActionKey action = TranslateActionKey(key);
            if (action == ActionKey::None)
            {
                return;
            }

            SetActionHeld(action, false);
            if (heldAction_ == action)
            {
                heldAction_ = ActionKey::None;
                for (ActionKey candidate : { ActionKey::Forward, ActionKey::Backward, ActionKey::TurnLeft, ActionKey::TurnRight })
                {
                    if (IsActionHeld(candidate))
                    {
                        heldAction_ = candidate;
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
        Direction facing_ = East;
        int steps_ = 0;
        int score_ = 0;
        int hintsPurchased_ = 0;
        bool showHintOverlay_ = false;
        ActionKey heldAction_ = ActionKey::None;
        bool actionHeld_[4] = { false, false, false, false };
        chrono::steady_clock::time_point startTime_{};
        chrono::steady_clock::time_point lastActionTime_{};
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
            facing_ = East;
            steps_ = 0;
            score_ = 0;
            hintsPurchased_ = 0;
            showHintOverlay_ = false;
            heldAction_ = ActionKey::None;
            fill(begin(actionHeld_), end(actionHeld_), false);
            activeHintCells_.clear();
            startTime_ = chrono::steady_clock::now();
            lastActionTime_ = startTime_;
        }

        void UpdateMenuLayout()
        {
            const int buttonWidth = 210;
            const int buttonHeight = 78;
            const int gap = 28;
            const int totalWidth = buttonWidth * static_cast<int>(difficulties_.size()) + gap * 2;
            const int startX = (clientWidth_ - totalWidth) / 2;
            const int top = clientHeight_ / 2;
            for (size_t index = 0; index < difficulties_.size(); ++index)
            {
                difficulties_[index].buttonRect.left = startX + static_cast<int>(index) * (buttonWidth + gap);
                difficulties_[index].buttonRect.top = top;
                difficulties_[index].buttonRect.right = difficulties_[index].buttonRect.left + buttonWidth;
                difficulties_[index].buttonRect.bottom = top + buttonHeight;
            }
        }

        ActionKey TranslateActionKey(WPARAM key) const
        {
            switch (key)
            {
            case VK_UP:
                return ActionKey::Forward;
            case VK_DOWN:
                return ActionKey::Backward;
            case VK_LEFT:
                return ActionKey::TurnLeft;
            case VK_RIGHT:
                return ActionKey::TurnRight;
            default:
                return ActionKey::None;
            }
        }

        int ActionIndex(ActionKey action) const
        {
            switch (action)
            {
            case ActionKey::Forward:
                return 0;
            case ActionKey::Backward:
                return 1;
            case ActionKey::TurnLeft:
                return 2;
            case ActionKey::TurnRight:
                return 3;
            default:
                return -1;
            }
        }

        bool IsActionHeld(ActionKey action) const
        {
            const int index = ActionIndex(action);
            return index >= 0 ? actionHeld_[index] : false;
        }

        void SetActionHeld(ActionKey action, bool held)
        {
            const int index = ActionIndex(action);
            if (index >= 0)
            {
                actionHeld_[index] = held;
            }
        }

        void PerformAction(ActionKey action)
        {
            switch (action)
            {
            case ActionKey::Forward:
                MoveRelative(0);
                break;
            case ActionKey::Backward:
                MoveRelative(2);
                break;
            case ActionKey::TurnLeft:
                facing_ = static_cast<Direction>((facing_ + 3) % 4);
                lastActionTime_ = chrono::steady_clock::now();
                break;
            case ActionKey::TurnRight:
                facing_ = static_cast<Direction>((facing_ + 1) % 4);
                lastActionTime_ = chrono::steady_clock::now();
                break;
            default:
                break;
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
            const int starCount = max(6, min(static_cast<int>(cells.size()) / 4, 30));
            for (int index = 0; index < starCount; ++index)
            {
                maze_[cells[index].y][cells[index].x].hasStar = true;
            }
        }

        Direction RelativeDirection(int offset) const
        {
            return static_cast<Direction>((facing_ + offset) % 4);
        }

        bool CanMove(Direction direction) const
        {
            return !maze_[playerRow_][playerCol_].walls[direction];
        }

        void MoveRelative(int offset)
        {
            const Direction direction = RelativeDirection(offset);
            if (!CanMove(direction))
            {
                lastActionTime_ = chrono::steady_clock::now();
                return;
            }

            static const int dr[4] = { -1, 0, 1, 0 };
            static const int dc[4] = { 0, 1, 0, -1 };
            playerRow_ += dr[direction];
            playerCol_ += dc[direction];
            ++steps_;
            lastActionTime_ = chrono::steady_clock::now();
            CollectStarIfPresent();
            if (showHintOverlay_)
            {
                RebuildHint();
            }
            if (playerRow_ == currentRows_ - 1 && playerCol_ == currentCols_ - 1)
            {
                screenState_ = ScreenState::Victory;
                heldAction_ = ActionKey::None;
                fill(begin(actionHeld_), end(actionHeld_), false);
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
                const POINT current = queue[index++];
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

        bool IsHintCell(int row, int col) const
        {
            for (const POINT& cell : activeHintCells_)
            {
                if (cell.x == col && cell.y == row)
                {
                    return true;
                }
            }
            return false;
        }

        vector<CorridorStep> BuildCorridorSteps() const
        {
            vector<CorridorStep> steps;
            int row = playerRow_;
            int col = playerCol_;
            Direction direction = facing_;

            for (int depth = 0; depth < 4; ++depth)
            {
                const Cell& cell = maze_[row][col];
                CorridorStep step{};
                step.leftWall = cell.walls[(direction + 3) % 4];
                step.rightWall = cell.walls[(direction + 1) % 4];
                step.frontWall = cell.walls[direction];
                step.hasStar = cell.hasStar;
                step.isGoal = (row == currentRows_ - 1 && col == currentCols_ - 1);
                step.isHint = showHintOverlay_ && IsHintCell(row, col);
                steps.push_back(step);

                if (cell.walls[direction] || depth == 3)
                {
                    break;
                }

                static const int dr[4] = { -1, 0, 1, 0 };
                static const int dc[4] = { 0, 1, 0, -1 };
                row += dr[direction];
                col += dc[direction];
            }

            return steps;
        }

        int ElapsedSeconds() const
        {
            return static_cast<int>(chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime_).count());
        }

        void FillRectColor(HDC hdc, const RECT& rect, COLORREF color) const
        {
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
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
            TextOutW(hdc, 40, 24, L"Maze Adventure FP", 17);
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
                : L"Up/Down: move  Left/Right: turn  J: buy partial hint  H: show/hide  R: restart  ESC: quit";
            TextOutW(hdc, 520, 56, controls.c_str(), static_cast<int>(controls.size()));

            SelectObject(hdc, oldFont);
            DeleteObject(titleFont);
            DeleteObject(textFont);
        }

        void DrawCenteredText(HDC hdc, const RECT& rect, const wstring& text) const
        {
            RECT local = rect;
            DrawTextW(hdc, text.c_str(), static_cast<int>(text.size()), &local, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
            const wstring subtitle = L"First-person maze view with visible hands, stars, and purchasable hint fragments.";
            TextOutW(hdc, clientWidth_ / 2 - 400, 250, subtitle.c_str(), static_cast<int>(subtitle.size()));

            RECT preview{ clientWidth_ / 2 - 240, 320, clientWidth_ / 2 + 240, 490 };
            HBRUSH previewBrush = CreateSolidBrush(RGB(227, 217, 189));
            HPEN previewPen = CreatePen(PS_SOLID, 2, RGB(115, 104, 85));
            HGDIOBJ oldBrush = SelectObject(hdc, previewBrush);
            HGDIOBJ oldPen = SelectObject(hdc, previewPen);
            RoundRect(hdc, preview.left, preview.top, preview.right, preview.bottom, 26, 26);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(previewBrush);
            DeleteObject(previewPen);
            DrawPreviewScene(hdc, preview);

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

        void DrawPreviewScene(HDC hdc, const RECT& rect)
        {
            RECT sky{ rect.left + 20, rect.top + 18, rect.right - 20, rect.top + 80 };
            RECT floor{ rect.left + 20, rect.top + 80, rect.right - 20, rect.bottom - 20 };
            FillRectColor(hdc, sky, kSkyColor);
            FillRectColor(hdc, floor, kFloorColor);

            POINT leftWall[4] = {
                { rect.left + 40, rect.top + 58 },
                { rect.left + 165, rect.top + 105 },
                { rect.left + 165, rect.bottom - 28 },
                { rect.left + 40, rect.bottom - 12 }
            };
            POINT rightWall[4] = {
                { rect.right - 40, rect.top + 58 },
                { rect.right - 165, rect.top + 105 },
                { rect.right - 165, rect.bottom - 28 },
                { rect.right - 40, rect.bottom - 12 }
            };
            POINT backWall[4] = {
                { rect.left + 185, rect.top + 98 },
                { rect.right - 185, rect.top + 98 },
                { rect.right - 185, rect.bottom - 34 },
                { rect.left + 185, rect.bottom - 34 }
            };
            DrawPolygon(hdc, leftWall, 4, kWallMidColor, kWallFarColor);
            DrawPolygon(hdc, rightWall, 4, kWallMidColor, kWallFarColor);
            DrawPolygon(hdc, backWall, 4, kWallFarColor, RGB(48, 48, 56));

            RECT leftDoor{ rect.left + 232, rect.top + 118, rect.left + 285, rect.bottom - 34 };
            RECT rightDoor{ rect.left + 285, rect.top + 118, rect.left + 338, rect.bottom - 34 };
            FillRectColor(hdc, leftDoor, kDoorDarkColor);
            FillRectColor(hdc, rightDoor, kDoorColor);

            POINT star[10];
            const int centerX = (rect.left + rect.right) / 2;
            const int centerY = rect.top + 86;
            for (int i = 0; i < 10; ++i)
            {
                const double angle = -3.14159265 / 2.0 + i * 3.14159265 / 5.0;
                const int radius = (i % 2 == 0) ? 13 : 6;
                star[i].x = static_cast<LONG>(centerX + cos(angle) * radius);
                star[i].y = static_cast<LONG>(centerY + sin(angle) * radius);
            }
            DrawPolygon(hdc, star, 10, kStarColor, RGB(170, 125, 33));
        }

        void DrawFirstPersonView(HDC hdc)
        {
            RECT viewRect{ 72, kInfoBarHeight + 36, clientWidth_ - 280, clientHeight_ - 54 };
            RECT skyRect{ viewRect.left, viewRect.top, viewRect.right, (viewRect.top + viewRect.bottom) / 2 };
            RECT groundRect{ viewRect.left, (viewRect.top + viewRect.bottom) / 2, viewRect.right, viewRect.bottom };
            FillRectColor(hdc, skyRect, kSkyColor);
            FillRectColor(hdc, groundRect, kFloorColor);

            HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(70, 63, 49));
            HGDIOBJ oldPen = SelectObject(hdc, borderPen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, viewRect.left, viewRect.top, viewRect.right, viewRect.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(borderPen);

            const vector<CorridorStep> steps = BuildCorridorSteps();
            const array<RECT, 4> layerRects = {
                RECT{ viewRect.left + 70, viewRect.top + 44, viewRect.right - 70, viewRect.bottom - 64 },
                RECT{ viewRect.left + 180, viewRect.top + 100, viewRect.right - 180, viewRect.bottom - 118 },
                RECT{ viewRect.left + 280, viewRect.top + 155, viewRect.right - 280, viewRect.bottom - 166 },
                RECT{ viewRect.left + 360, viewRect.top + 205, viewRect.right - 360, viewRect.bottom - 210 }
            };

            for (int depth = static_cast<int>(steps.size()) - 1; depth >= 0; --depth)
            {
                const CorridorStep& step = steps[depth];
                const RECT& rect = layerRects[min(depth, static_cast<int>(layerRects.size()) - 1)];
                const COLORREF wallColor = depth == 0 ? kWallNearColor : (depth == 1 ? kWallMidColor : kWallFarColor);

                if (step.leftWall)
                {
                    POINT leftWall[4] = {
                        { viewRect.left, viewRect.top },
                        { rect.left, rect.top },
                        { rect.left, rect.bottom },
                        { viewRect.left, viewRect.bottom }
                    };
                    DrawPolygon(hdc, leftWall, 4, wallColor, RGB(48, 48, 56));
                }

                if (step.rightWall)
                {
                    POINT rightWall[4] = {
                        { viewRect.right, viewRect.top },
                        { rect.right, rect.top },
                        { rect.right, rect.bottom },
                        { viewRect.right, viewRect.bottom }
                    };
                    DrawPolygon(hdc, rightWall, 4, wallColor, RGB(48, 48, 56));
                }

                if (step.frontWall)
                {
                    FillRectColor(hdc, rect, wallColor);
                    HPEN frontPen = CreatePen(PS_SOLID, 1, RGB(48, 48, 56));
                    oldPen = SelectObject(hdc, frontPen);
                    oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
                    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
                    SelectObject(hdc, oldPen);
                    SelectObject(hdc, oldBrush);
                    DeleteObject(frontPen);
                }

                if (step.isHint)
                {
                    RECT hintRect{
                        rect.left + (rect.right - rect.left) / 3,
                        rect.top + (rect.bottom - rect.top) / 3,
                        rect.right - (rect.right - rect.left) / 3,
                        rect.bottom - (rect.bottom - rect.top) / 3
                    };
                    FillRectColor(hdc, hintRect, kHintColor);
                }

                if (step.hasStar)
                {
                    DrawStarAt(hdc, (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 - 20, max(8, 14 - depth * 2));
                }

                if (step.isGoal)
                {
                    DrawDoorAt(hdc, rect);
                }
            }

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(246, 242, 232));
            HFONT font = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(hdc, font);
            const wstring facingText = FacingText();
            TextOutW(hdc, viewRect.left + 18, viewRect.top + 14, facingText.c_str(), static_cast<int>(facingText.size()));
            SelectObject(hdc, oldFont);
            DeleteObject(font);
        }

        void DrawDoorAt(HDC hdc, const RECT& rect) const
        {
            const int width = (rect.right - rect.left) / 2;
            const int left = (rect.left + rect.right) / 2 - width / 2;
            const int right = left + width;
            const int top = rect.top + 24;
            const int bottom = rect.bottom - 10;
            RECT leftDoor{ left, top, left + width / 2, bottom };
            RECT rightDoor{ left + width / 2, top, right, bottom };
            FillRectColor(hdc, leftDoor, kDoorDarkColor);
            FillRectColor(hdc, rightDoor, kDoorColor);
            FillCircle(hdc, rightDoor.left + 14, (top + bottom) / 2, 4, RGB(228, 192, 94), RGB(108, 84, 39));
        }

        void DrawStarAt(HDC hdc, int centerX, int centerY, int radius) const
        {
            POINT points[10];
            for (int index = 0; index < 10; ++index)
            {
                const double angle = -3.14159265 / 2.0 + index * 3.14159265 / 5.0;
                const int useRadius = (index % 2 == 0) ? radius : max(4, radius / 2);
                points[index].x = static_cast<LONG>(centerX + cos(angle) * useRadius);
                points[index].y = static_cast<LONG>(centerY + sin(angle) * useRadius);
            }
            DrawPolygon(hdc, points, 10, kStarColor, RGB(171, 122, 30));
        }

        void DrawPlayerHands(HDC hdc)
        {
            const int baseY = clientHeight_ - 120;
            POINT leftSleeve[4] = {
                { 120, baseY + 18 },
                { 250, baseY - 34 },
                { 330, baseY + 76 },
                { 176, baseY + 118 }
            };
            POINT rightSleeve[4] = {
                { clientWidth_ - 120, baseY + 18 },
                { clientWidth_ - 250, baseY - 34 },
                { clientWidth_ - 330, baseY + 76 },
                { clientWidth_ - 176, baseY + 118 }
            };
            POINT leftHand[4] = {
                { 266, baseY + 44 },
                { 325, baseY + 26 },
                { 350, baseY + 84 },
                { 290, baseY + 104 }
            };
            POINT rightHand[4] = {
                { clientWidth_ - 266, baseY + 44 },
                { clientWidth_ - 325, baseY + 26 },
                { clientWidth_ - 350, baseY + 84 },
                { clientWidth_ - 290, baseY + 104 }
            };
            DrawPolygon(hdc, leftSleeve, 4, kSleeveColor, RGB(32, 62, 118));
            DrawPolygon(hdc, rightSleeve, 4, kSleeveColor, RGB(32, 62, 118));
            DrawPolygon(hdc, leftHand, 4, kArmColor, RGB(138, 109, 90));
            DrawPolygon(hdc, rightHand, 4, kArmColor, RGB(138, 109, 90));
        }
        void DrawMiniMap(HDC hdc)
        {
            RECT panel{ clientWidth_ - 240, kInfoBarHeight + 36, clientWidth_ - 36, kInfoBarHeight + 276 };
            HBRUSH panelBrush = CreateSolidBrush(RGB(244, 238, 224));
            HPEN panelPen = CreatePen(PS_SOLID, 2, RGB(113, 101, 80));
            HGDIOBJ oldBrush = SelectObject(hdc, panelBrush);
            HGDIOBJ oldPen = SelectObject(hdc, panelPen);
            RoundRect(hdc, panel.left, panel.top, panel.right, panel.bottom, 24, 24);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(panelBrush);
            DeleteObject(panelPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, kTextColor);
            HFONT font = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(hdc, font);
            TextOutW(hdc, panel.left + 18, panel.top + 16, L"Mini Map", 8);
            SelectObject(hdc, oldFont);
            DeleteObject(font);

            const int mapLeft = panel.left + 18;
            const int mapTop = panel.top + 52;
            const int mapWidth = panel.right - panel.left - 36;
            const int mapHeight = panel.bottom - panel.top - 68;
            const int cellSize = max(4, min(mapWidth / currentCols_, mapHeight / currentRows_));
            HPEN linePen = CreatePen(PS_SOLID, 1, RGB(74, 74, 80));
            oldPen = SelectObject(hdc, linePen);

            for (int row = 0; row < currentRows_; ++row)
            {
                for (int col = 0; col < currentCols_; ++col)
                {
                    RECT cellRect{
                        mapLeft + col * cellSize,
                        mapTop + row * cellSize,
                        mapLeft + (col + 1) * cellSize,
                        mapTop + (row + 1) * cellSize
                    };
                    FillRectColor(hdc, cellRect, kMiniPathColor);
                    if (maze_[row][col].hasStar)
                    {
                        RECT starRect = cellRect;
                        InflateRect(&starRect, -1, -1);
                        FillRectColor(hdc, starRect, kMiniStarColor);
                    }
                    if (row == currentRows_ - 1 && col == currentCols_ - 1)
                    {
                        FillRectColor(hdc, cellRect, kMiniGoalColor);
                    }
                    if (row == playerRow_ && col == playerCol_)
                    {
                        FillRectColor(hdc, cellRect, kMiniPlayerColor);
                    }

                    if (maze_[row][col].walls[North])
                    {
                        MoveToEx(hdc, cellRect.left, cellRect.top, nullptr);
                        LineTo(hdc, cellRect.right, cellRect.top);
                    }
                    if (maze_[row][col].walls[West])
                    {
                        MoveToEx(hdc, cellRect.left, cellRect.top, nullptr);
                        LineTo(hdc, cellRect.left, cellRect.bottom);
                    }
                    if (maze_[row][col].walls[East])
                    {
                        MoveToEx(hdc, cellRect.right, cellRect.top, nullptr);
                        LineTo(hdc, cellRect.right, cellRect.bottom);
                    }
                    if (maze_[row][col].walls[South])
                    {
                        MoveToEx(hdc, cellRect.left, cellRect.bottom, nullptr);
                        LineTo(hdc, cellRect.right, cellRect.bottom);
                    }
                }
            }

            SelectObject(hdc, oldPen);
            DeleteObject(linePen);
        }

        wstring FacingText() const
        {
            switch (facing_)
            {
            case North:
                return L"Facing: North";
            case East:
                return L"Facing: East";
            case South:
                return L"Facing: South";
            case West:
                return L"Facing: West";
            default:
                return L"Facing: ?";
            }
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
            TextOutW(hdc, overlay.left + 190, overlay.top + 22, L"Victory!", 8);
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
        case WM_ERASEBKGND:
            return 1;
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
    const wchar_t kClassName[] = L"MazeAdventureFPWindow";

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
        L"Maze Adventure FP",
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
