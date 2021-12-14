package timogr.app;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.text.Editable;
import android.text.SpannableStringBuilder;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import static android.view.WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN;

import static android.view.inputmethod.EditorInfo.IME_ACTION_DONE;
import static android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI;
import static android.view.inputmethod.EditorInfo.IME_FLAG_NO_FULLSCREEN;

// https://programming.vip/docs/android-adjustnothing-get-keyboard-height.html

public class TimogrActivity extends android.app.NativeActivity {
    private InputMethodManager imm = null;
    private View focusView;
    private Handler handler = new Handler();

    public TimogrActivity() {
        super();
    }

    public void showKeyboard(boolean show) {
        handler.post(new Runnable() {
            public void run() {
                Window window = getWindow();
                if (show) {
                    imm.showSoftInput(focusView, InputMethodManager.SHOW_IMPLICIT);
                } else {
                    android.os.IBinder token = focusView.getWindowToken();
                    imm.hideSoftInputFromWindow(token, 0);
                }        
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        imm = getSystemService(InputMethodManager.class);

        Window window = getWindow();
        window.setSoftInputMode(SOFT_INPUT_STATE_HIDDEN | SOFT_INPUT_ADJUST_NOTHING);

        focusView = new FocusableView(this);
        ViewGroup.LayoutParams params =
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        window.addContentView(focusView, params);
    }

    @Override
    protected void onResume() {
        super.onResume();
        focusView.requestFocus();
    }
}

class TimogrInputConnection extends BaseInputConnection {
    private final Editable editable;

    TimogrInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
        editable = new SpannableStringBuilder(){
            public SpannableStringBuilder replace(int st, int en, CharSequence text, int start, int end) {
                if (end - start == 1 && text.length() > 0) {
                    char ch = text.charAt(start);
                    TimogrInputConnection.this.synthesizeKeyEvent(ch);
                }
                return this;
            }
        };
    }

    private void synthesizeKeyEvent(char ch) {
        long now = android.os.SystemClock.uptimeMillis();
        super.sendKeyEvent(new KeyEvent(
                now,
                now,
                KeyEvent.ACTION_DOWN,
                0,
                0,
                0,
                // Virtual keyboard
                -1,
                // Send character as scan code !
                ch
        ));
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        int unicodeChar = event.getUnicodeChar();
        return super.sendKeyEvent(new KeyEvent(
                event.getDownTime(),
                event.getEventTime(),
                event.getAction(),
                event.getKeyCode(),
                event.getRepeatCount(),
                event.getMetaState(),
                event.getDeviceId(),
                // Send unicode char as scan code!
                unicodeChar
        ));
    }

    @Override
    public Editable getEditable() {
        return editable;
    }
}

class FocusableView extends View {
    public FocusableView(Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.imeOptions = IME_ACTION_DONE | IME_FLAG_NO_FULLSCREEN | IME_FLAG_NO_EXTRACT_UI;
        return new TimogrInputConnection(this, true);
    }

    public boolean onCheckIsTextEditor() {
        return true;
    }
}
