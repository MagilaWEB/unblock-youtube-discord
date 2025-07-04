#pragma once
#include <conio.h>

// Taken here https://gist.github.com/lyf-is-coding/2f3806f68d2c2b66f687216e01667a36

enum class VK : u32
{
	LBUTTON = 0x01,	   // Left mouse button
	RBUTTON = 0x02,	   // Right mouse button
	CANCEL	= 0x03,	   // Control-break processing
	MBUTTON = 0x04,	   // Middle mouse button (three-button mouse)
	// 0x07 Undefined
	BACK	= 0x08,	   // BACKSPACE key
	TAB		= 0x09,	   // TAB key
	// 0x0A-0B Reserved
	CLEAR	= 0x0C,	   // CLEAR key
	ENTER	= 0x0D,	   // ENTER key

	MENU				= 0x12,	   // ALT key
	PAUSE				= 0x13,	   // PAUSE key
	CAPITAL				= 0x14,	   // CAPS LOCK key
	KANA				= 0x15,	   // IME Kana mode
	HANGUEL				= 0x15,	   // IME Hanguel mode (maintained for compatibility; use `VK_HANGUL`)
	HANGUL				= 0x15,	   // IME Hangul mode
	IME_ON				= 0x16,	   // IME On
	JUNJA				= 0x17,	   // IME Junja mode
	FINAL				= 0x18,	   // IME final mode
	HANJA				= 0x19,	   // IME Hanja mode
	KANJI				= 0x19,	   // IME Kanji mode
	IME_OFF				= 0x1A,	   // IME Off
	ESCAPE				= 0x1B,	   // ESC key
	CONVERT				= 0x1C,	   // IME convert
	NONCONVERT			= 0x1D,	   // IME nonconvert
	ACCEPT				= 0x1E,	   // IME accept
	MODECHANGE			= 0x1F,	   // IME mode change request
	SPACE				= 0x20,	   // SPACEBAR
	PRIOR				= 0x21,	   // PAGE UP key
	NEXT				= 0x22,	   // PAGE DOWN key
	END					= 0x23,	   // END key
	HOME				= 0x24,	   // HOME key
	LEFT				= 0x25,	   // LEFT ARROW key
	UP					= 0x26,	   // UP ARROW key
	RIGHT				= 0x27,	   // RIGHT ARROW key
	DOWN				= 0x28,	   // DOWN ARROW key
	SELECT				= 0x29,	   // SELECT key
	PRINT				= 0x2A,	   // PRINT key
	EXECUTE				= 0x2B,	   // EXECUTE key
	SNAPSHOT			= 0x2C,	   // PRINT SCREEN key
	INSERT				= 0x2D,	   // INS key
	HELP				= 0x2F,	   // HELP key
	NUM0				= 0x30,	   // 0 key
	NUM1				= 0x31,	   // 1 key
	NUM2				= 0x32,	   // 2 key
	NUM3				= 0x33,	   // 3 key
	NUM4				= 0x34,	   // 4 key
	NUM5				= 0x35,	   // 5 key
	NUM6				= 0x36,	   // 6 key
	NUM7				= 0x37,	   // 7 key
	NUM8				= 0x38,	   // 8 key
	NUM9				= 0x39,	   // 9 key
	// 0x3A-40 Undefined
	A					= 0x41,	   // A key
	B					= 0x42,	   // B key
	C					= 0x43,	   // C key
	D					= 0x44,	   // D key
	E					= 0x45,	   // E key
	F					= 0x46,	   // F key
	G					= 0x47,	   // G key
	H					= 0x48,	   // H key
	I					= 0x49,	   // I key
	J					= 0x4A,	   // J key
	K					= 0x4B,	   // K key
	L					= 0x4C,	   // L key
	M					= 0x4D,	   // M key
	N					= 0x4E,	   // N key
	O					= 0x4F,	   // O key
	P					= 0x50,	   // P key
	Q					= 0x51,	   // Q key
	R					= 0x52,	   // R key
	S					= 0x53,	   // S key
	T					= 0x54,	   // T key
	U					= 0x55,	   // U key
	V					= 0x56,	   // V key
	W					= 0x57,	   // W key
	X					= 0x58,	   // X key
	Y					= 0x59,	   // Y key
	Z					= 0x5A,	   // Z key
	LWIN				= 0x5B,	   // Left Windows key (Natural keyboard)
	RWIN				= 0x5C,	   // Right Windows key (Natural keyboard)
	APPS				= 0x5D,	   // Applications key (Natural keyboard)
	// 0x5E Reserved
	SLEEP				= 0x5F,	   // Computer Sleep key
	NUMPAD0				= 0x60,	   // Numeric keypad 0 key
	NUMPAD1				= 0x61,	   // Numeric keypad 1 key
	NUMPAD2				= 0x62,	   // Numeric keypad 2 key
	NUMPAD3				= 0x63,	   // Numeric keypad 3 key
	NUMPAD4				= 0x64,	   // Numeric keypad 4 key
	NUMPAD5				= 0x65,	   // Numeric keypad 5 key
	NUMPAD6				= 0x66,	   // Numeric keypad 6 key
	NUMPAD7				= 0x67,	   // Numeric keypad 7 key
	NUMPAD8				= 0x68,	   // Numeric keypad 8 key
	NUMPAD9				= 0x69,	   // Numeric keypad 9 key
	MULTIPLY			= 0x6A,	   // Multiply key
	ADD					= 0x6B,	   // Add key
	SEPARATOR			= 0x6C,	   // Separator key
	SUBTRACT			= 0x6D,	   // Subtract key
	DECIMAL				= 0x6E,	   // Decimal key
	DIVIDE				= 0x6F,	   // Divide key
	F1					= 0x70,	   // F1 key
	F2					= 0x71,	   // F2 key
	F3					= 0x72,	   // F3 key
	F4					= 0x73,	   // F4 key
	F5					= 0x74,	   // F5 key
	F6					= 0x75,	   // F6 key
	F7					= 0x76,	   // F7 key
	F8					= 0x77,	   // F8 key
	F9					= 0x78,	   // F9 key
	F10					= 0x79,	   // F10 key
	F11					= 0x7A,	   // F11 key
	F12					= 0x7B,	   // F12 key
	F13					= 0x7C,	   // F13 key
	F14					= 0x7D,	   // F14 key
	F15					= 0x7E,	   // F15 key
	F16					= 0x7F,	   // F16 key
	F17					= 0x80,	   // F17 key
	F18					= 0x81,	   // F18 key
	F19					= 0x82,	   // F19 key
	F20					= 0x83,	   // F20 key
	F21					= 0x84,	   // F21 key
	F22					= 0x85,	   // F22 key
	F23					= 0x86,	   // F23 key
	F24					= 0x87,	   // F24 key
	// 0x88-8F Unassigned
	NUMLOCK				= 0x90,	   // NUM LOCK key
	SCROLL				= 0x91,	   // SCROLL LOCK key
	// 0x92-96 OEM specific
	// 0x97-9F Unassigned
	LSHIFT				= 0xA0,	   // Left SHIFT key
	RSHIFT				= 0xA1,	   // Right SHIFT key
	LCONTROL			= 0xA2,	   // Left CONTROL key
	RCONTROL			= 0xA3,	   // Right CONTROL key
	LMENU				= 0xA4,	   // Left ALT key
	RMENU				= 0xA5,	   // Right ALT key
	BROWSER_BACK		= 0xA6,	   // Browser Back key
	BROWSER_FORWARD		= 0xA7,	   // Browser Forward key
	BROWSER_REFRESH		= 0xA8,	   // Browser Refresh key
	BROWSER_STOP		= 0xA9,	   // Browser Stop key
	BROWSER_SEARCH		= 0xAA,	   // Browser Search key
	BROWSER_FAVORITES	= 0xAB,	   // Browser Favorites key
	BROWSER_HOME		= 0xAC,	   // Browser Start and Home key
	VOLUME_MUTE			= 0xAD,	   // Volume Mute key
	VOLUME_DOWN			= 0xAE,	   // Volume Down key
	VOLUME_UP			= 0xAF,	   // Volume Up key
	MEDIA_NEXT_TRACK	= 0xB0,	   // Next Track key
	MEDIA_PREV_TRACK	= 0xB1,	   // Previous Track key
	MEDIA_STOP			= 0xB2,	   // Stop Media key
	MEDIA_PLAY_PAUSE	= 0xB3,	   // Play/Pause Media key
	LAUNCH_MAIL			= 0xB4,	   // Start Mail key
	LAUNCH_MEDIA_SELECT = 0xB5,	   // Select Media key
	LAUNCH_APP1			= 0xB6,	   // Start Application 1 key
	LAUNCH_APP2			= 0xB7,	   // Start Application 2 key
	// 0xB8-B9 Reserved
	OEM_1				= 0xBA,	   // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the ';:' key
	OEM_PLUS			= 0xBB,	   // For any country/region, the '+' key
	OEM_COMMA			= 0xBC,	   // For any country/region, the ',' key
	OEM_MINUS			= 0xBD,	   // For any country/region, the '-' key
	OEM_PERIOD			= 0xBE,	   // For any country/region, the '.' key
	OEM_2				= 0xBF,	   // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '/?' key
	OEM_3				= 0xC0,	   // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '\`~' key
	// 0xC1-D7 Reserved
	// 0xD8-DA Unassigned
	OEM_4				= 0xDB,	   // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '\[{' key
	OEM_5				= 0xDC,	   // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '\\\|' key
	OEM_6				= 0xDD,	   // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '\]}' key
	OEM_7 = 0xDE,	 // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the 'single-quote/double-quote' key
	OEM_8 = 0xDF,	 // Used for miscellaneous characters; it can vary by keyboard.
	// 0xE0 Reserved
	// 0xE1 OEM specific
	OEM_102	   = 0xE2,	  // The `<>` keys on the US standard keyboard, or the `\\|` key on the non-US 102-key keyboard
	// 0xE3-E4 OEM specific
	PROCESSKEY = 0xE5,	  // IME PROCESS key
	// 0xE6 OEM specific
	PACKET = 0xE7,	  // Used to pass Unicode characters as if they were keystrokes. The `VK_PACKET` key is the low word of a 32-bit Virtual Key value
					  // used for non-keyboard input methods. For more information, see Remark in
					  // [`KEYBDINPUT`](/windows/win32/api/winuser/ns-winuser-keybdinput),
					  // [`SendInput`](/windows/win32/api/winuser/nf-winuser-sendinput), [`WM_KEYDOWN`](wm-keydown.md), and [`WM_KEYUP`](wm-keyup.md)
	// 0xE8 Unassigned
	// 0xE9-F5 OEM specific
	ATTN   = 0xF6,	  // Attn key
	CRSEL  = 0xF7,	  // CrSel key
	EXSEL  = 0xF8,	  // ExSel key
	EREOF  = 0xF9,	  // Erase EOF key
	PLAY   = 0xFA,	  // Play key
	ZOOM   = 0xFB,	  // Zoom key
	NONAME = 0xFC,	  // Reserved
	PA1	   = 0xFD	  // PA1 key
};

template<typename TResult = void>
	requires std::is_same_v<TResult, void> || std::is_same_v<TResult, bool>
inline static TResult KEY(VK key_num, bool wait_key_click)
{
	static_assert(std::is_same_v<TResult, void> || std::is_same_v<TResult, bool>, "The KEY function supports only void or bool as a return result.");

	const u32 num = static_cast<u32>(key_num);
	do
	{
		if (GetKeyState(num) & 0x80'00)
		{
			// We are waiting for them to stop holding key.
			while (true)
			{
				if (!(GetKeyState(num) & 0x80'00))
					if constexpr (std::is_same_v<TResult, bool>)
						return true;
					else
						return;

				std::this_thread::yield();
			}
		}

		if (wait_key_click)
			std::this_thread::yield();

	} while (wait_key_click);

	if constexpr (std::is_same_v<TResult, bool>)
		return false;
	else
		return;
}
