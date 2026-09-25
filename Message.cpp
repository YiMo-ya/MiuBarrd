#include "Message.h"
#include "Shared.h"

NOXS;

RenWin MessageWindow;
static const Color MessageColor[6] =
{
	Color(200,150,255),//调试
	Color(180,200,255),//信息
	Color(240,130,190),//疑问
	Color(255,70,70),//错误
	Color(100,255,100),//成功
	Color(255,255,100)//警告
};
IMAGE Ico;

static void InitMessageWindow(int r,int g,int b, IconType IcoType)
{
	//创建窗口
#pragma region MyRegion
	XWindow::CreateGraphWindow(MessageWindow, -1, -1, ScreenSize.x * 2 / 5, ScreenSize.y / 6, "Animata",Style::Close);
	XWindow::SetIcon(MessageWindow, ImgPath + L"\\Icon.dll");

    XWindow::RemoveWindowStyle(MessageWindow, WS_MINIMIZEBOX);
    XWindow::RemoveWindowStyle(MessageWindow, WS_MAXIMIZEBOX);

    XWindow::DWM::SetWindowBackType(MessageWindow, BACKTYPE_BLUR);
    XWindow::DWM::SetWindowDarkMode(MessageWindow, true);
    XWindow::DWM::ExtendIntoClientArea(MessageWindow, -1, -1, -1, -1);
    XWindow::DWM::SetWindowBorderColor(MessageWindow, MessageColor[IcoType]);
    XWindow::DWM::SetWindowTitleBarColor(MessageWindow, MessageColor[IcoType]);

	//置顶窗口
	SetWindowLongPtrW(MessageWindow.getNativeHandle(), GWL_EXSTYLE,
		WS_EX_TOPMOST);
	SetWindowPos(MessageWindow.getNativeHandle(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

#pragma endregion

    //加载图标
#pragma region MyRegion
    if (IcoType == ICOTYPE_DEBUG) XImage::NewImage(Ico, ImgPath + L"\\Msg\\Debug.dll");
	if (IcoType == ICOTYPE_INFO) XImage::NewImage(Ico, ImgPath + L"\\Msg\\Info.dll");
    if (IcoType == ICOTYPE_WARNING) XImage::NewImage(Ico, ImgPath + L"\\Msg\\Warning.dll");
    if (IcoType == ICOTYPE_ERROR) XImage::NewImage(Ico, ImgPath + L"\\Msg\\Error.dll");
    if (IcoType == ICOTYPE_SUCCESS) XImage::NewImage(Ico, ImgPath + L"\\Msg\\Success.dll");
    if (IcoType == ICOTYPE_QUESTION) XImage::NewImage(Ico, ImgPath + L"\\Msg\\Question.dll");
#pragma endregion


	
	MessageWindow.requestFocus();
}

static std::vector<std::string> WrapUtf8ToLines(const std::string& utf8, int fontSize, float maxPixelWidth) {
    std::vector<std::string> lines;
    if (utf8.empty()) {
        return lines;
    }
    std::wstring wide = XString::Convert::utf8_to_wstring(utf8);
    if (wide.empty()) {
        lines.push_back(utf8);
        return lines;
    }

    std::wstring cur;
    float acc = 0.0f;
    const float asciiFactor = 0.6f;
    const float wideFactor = 1.0f;

    for (wchar_t ch : wide) {
        float cw = (ch <= 0x7F) ? (fontSize * asciiFactor) : (fontSize * wideFactor);
        if (!cur.empty() && acc + cw > maxPixelWidth) {
            lines.push_back(XString::Convert::wstring_to_utf8(cur));
            cur.clear();
            acc = 0.0f;
        }
        cur.push_back(ch);
        acc += cw;
    }
    if (!cur.empty()) {
        lines.push_back(XString::Convert::wstring_to_utf8(cur));
    }

    // 防止单行无法拆分（maxPixelWidth 过小）的情况，保证至少一行存在
    if (lines.empty()) lines.push_back(utf8);

    return lines;
}

static void PlayAudio(int audiotype)
{
    if (audiotype > 0)
    {
        switch (audiotype)
        {
        case 1:
        {
            PlaySoundA("SystemDefault", NULL, SND_ALIAS | SND_ASYNC);
            break;
        }
        case 2:
        {
            PlaySoundA("SystemExclamation", NULL, SND_ALIAS | SND_ASYNC);
            break;
        }
        case 3:
        {
            PlaySoundA("SystemHand", NULL, SND_ALIAS | SND_ASYNC);
            break;
        }
        default:
        {
            break;
        }
        }
    }
}

int Message::ShowMessage(MsgS msg,std::wstring WindowTitle)
{
    HWND pWindow = NULL;
    if(WindowTitle != L"NULL") pWindow = FindWindowW(NULL, WindowTitle.c_str());

	InitMessageWindow(MessageColor[msg.Ico].r, MessageColor[msg.Ico].g, MessageColor[msg.Ico].b, msg.Ico);

    if (msg.Ico != ICOTYPE_DEBUG) msg.title = "MiuBarrd：" + msg.title;
    else msg.title = "MiuBarrd Debug Terminal：" + msg.title;

	XWindow::SetTitle(MessageWindow, msg.title);

    //去掉尺寸按钮
    LONG style = GetWindowLongPtr(MessageWindow.getNativeHandle(), GWL_STYLE);
    style &= ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLongPtr(MessageWindow.getNativeHandle(), GWL_STYLE, style);

	Vector2i SCREENSIZE = XSystem::Info::GetScreenSize();
	Vector2u windowSize = MessageWindow.getSize();
	int w = windowSize.x;
	int h = windowSize.y;

	//计算按钮大小，按钮宽度根据窗口宽度和按钮数量动态调整，按钮高度固定为窗口高度的1/5
	int buttonWidth = min(w * 5 / 8 / msg.buttons.size(), w / 5);
	int buttonHeight = h / 5;

	//结果，默认为0，表示第一个按钮被点击或者按下回车键，如果有多个按钮，后续按钮依次为1、2、3...以此类推
	int result = msg.buttons.size() - 1;

    //是否退出
	bool isExit = false;

    PlayAudio(msg.audio);
    
    Color LastBkColor = XWindow::GetBackGroundColor();

    while (!XMsg::IsClose(MessageWindow) && !isExit)
    {
        XWindow::DelayFps(MessageWindow);

       XGraph::SetFillColor(Color(10, 10, 10, 100));
       XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, ScreenSize.x * 2 / 5, ScreenSize.y / 6, MessageWindow);

        //显示图标
		Ico.color = MessageColor[msg.Ico];
        XImage::PutScaleImage(Ico, w / 10, h / 2, w / 8 / 512.0, w / 8 / 512.0, MessageWindow, 0.5, 0.5);

        // --- 自动换行渲染逻辑开始 ---
#pragma region MyRegion
        int fontSize = SCREENSIZE.x / 100;
        XText::SetFontSize(fontSize);

        // 计算可用文本宽度：
        // 文本起始 x 为 w/5，右侧保留按钮区域（按按钮绘制时的起点和间隔估算）
        float leftX = static_cast<float>(w) / 5.0f;
        // 估算保留给按钮的宽度（与绘制按钮时保持一致）
        float maxTextWidth = static_cast<float>(w) - leftX * 1.1;
        if (maxTextWidth < 50.0f) maxTextWidth = static_cast<float>(w) - leftX - 10.0f; // 容错

        // 使用简单的像素估算换行（基于字符类型），生成多行
        std::vector<std::string> lines = WrapUtf8ToLines(msg.text, fontSize, maxTextWidth);

        // 绘制多行：垂直从 top 对齐开始
        XText::SetFontAdjust(ADJUST_LEFT, ADJUST_TOP);
        float startY = static_cast<float>(h) / 9.0f + h / 10;
        float lineHeight = fontSize * 1.35f; // 行高因子，可调
        XText::SetFontColor(Color::White);
        for (size_t li = 0; li < lines.size(); ++li) {
            float y = startY + li * lineHeight;
            XText::Xyprintf(leftX, y, lines[li], MessageWindow);
        }
#pragma endregion
        // --- 自动换行渲染逻辑结束 ---

        for (int i = 0; i < msg.buttons.size(); i++)
        {
            if (i == 0)
            {
                XGraph::SetFillColor(MessageColor[msg.Ico]);
                XGraph::RectangleShape::FillRoundRect_WithoutBorder(w - buttonWidth * (i + 1) * 6 / 5, h - buttonHeight * 7 / 5,
                    buttonWidth, buttonHeight, buttonHeight / 3, MessageWindow);
            }
            else
            {
                XGraph::SetColor(Color(130,130,130));
                XGraph::LineShape::SetLineWidth(SCREENSIZE.x / 500);
                XGraph::RectangleShape::RoundRect(w - buttonWidth * (i + 1) * 6 / 5, h - buttonHeight * 7 / 5,
                    buttonWidth, buttonHeight, buttonHeight / 3, MessageWindow);
            }

            XText::SetFontSize(SCREENSIZE.x / 100);
            XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
            XText::SetFontColor(i == 0 ? Color::Black : Color::White);
            XText::Xyprintf(w - buttonWidth * (i + 1) * 6 / 5 + buttonWidth / 2,
                h - buttonHeight * 7 / 5 + buttonHeight / 2, msg.buttons[i], MessageWindow);

            if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft) && XMsg::MouseMsg::IsMouseIn(w - buttonWidth * (i + 1) * 6 / 5, h - buttonHeight * 7 / 5,
                buttonWidth, buttonHeight))
            {
                result = i;
                isExit = true;
                break;
            }
        }

        if (pWindow == GetForegroundWindow())
        {
            MessageWindow.requestFocus();
            PlayAudio(msg.audio);
            XWindow::FlashWindow(MessageWindow);
        }

        if (XMsg::KeyMsg::Keystate(VK::Enter))
        {
            result = 0;
            isExit = true;
        }
    }

    //清理消息并返回
    MessageWindow.close();
    XMsg::ResetCloseMsg();

    XMsg::ClearMsg();
    XMsg::SetSleepTime(10);

    XWindow::SetBackGroundColor(LastBkColor);

    return result;
}

int Message::ShowMessage(std::string text, std::string title,
    IconType ico,
    std::vector<std::string> button,
    int audio,std::wstring WindowTitle)
{
    MsgS msg;
    msg.text = text;
    msg.title = title;
    msg.buttons = button;
    msg.Ico = ico;
    msg.audio = audio;
    return ShowMessage(msg, WindowTitle);
}