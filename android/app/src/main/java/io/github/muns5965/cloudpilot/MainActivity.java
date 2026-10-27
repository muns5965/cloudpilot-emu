package io.github.muns5965.cloudpilot;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.DateFormat;
import java.util.Date;

/**
 * Start screen (test build): pick a ROM with the system file picker, copy it to app storage, and
 * start the emulator. A saved TG50 session (files/tg50.ses, written by the emulator when it goes
 * to the background or quits) is resumed; without one a TG50 ROM boots cold. Anything else shows
 * the SDL test screen.
 */
public class MainActivity extends Activity {
    private static final String TAG = "cp-android";
    private static final int PICK_ROM = 1;
    private static final long MAX_ROM_SIZE = 64L * 1024 * 1024;

    private TextView status;
    private Button run;
    private Button deleteSession;

    private File romFile() {
        return new File(getFilesDir(), "rom.bin");
    }

    private File sessionFile() {
        return new File(getFilesDir(), "tg50.ses");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        final int pad = (int) (16 * getResources().getDisplayMetrics().density);
        layout.setPadding(pad, pad, pad, pad);
        layout.setOnApplyWindowInsetsListener((view, insets) -> {
            int top, bottom;
            if (Build.VERSION.SDK_INT >= 30) {
                android.graphics.Insets bars = insets.getInsets(WindowInsets.Type.systemBars());
                top = bars.top;
                bottom = bars.bottom;
            } else {
                top = insets.getSystemWindowInsetTop();
                bottom = insets.getSystemWindowInsetBottom();
            }
            view.setPadding(pad, pad + top, pad, pad + bottom);
            return insets;
        });

        TextView title = new TextView(this);
        title.setText("CloudpilotEmu Android 입력 진단 빌드 (2-E1-diag)\nTG50 ROM만 부팅합니다. 백그라운드로 가거나 종료할 때 세션을 저장하고, 다음 실행 때 이어서 시작합니다.");
        title.setTextSize(18);
        layout.addView(title);

        status = new TextView(this);
        status.setPadding(0, pad, 0, pad);
        layout.addView(status);

        Button pick = new Button(this);
        pick.setText("ROM 선택");
        pick.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, PICK_ROM);
        });
        layout.addView(pick);

        run = new Button(this);
        run.setText("실행 (세션이 있으면 이어서, 없으면 ROM 부팅)");
        run.setOnClickListener(v -> startEmulator(true));
        layout.addView(run);

        deleteSession = new Button(this);
        deleteSession.setText("저장된 세션 지우기");
        deleteSession.setOnClickListener(v -> new AlertDialog.Builder(this)
                .setMessage("저장된 세션(Palm 데이터·설정·설치한 프로그램)을 지웁니다. 다음 실행은 ROM으로 처음부터 부팅합니다.")
                .setPositiveButton("지우기", (d, w) -> {
                    boolean deleted = sessionFile().delete();
                    updateStatus(deleted ? "세션을 지웠습니다" : "세션을 지우지 못했습니다");
                })
                .setNegativeButton("취소", null)
                .show());
        layout.addView(deleteSession);

        // Diagnostic build only: IME events recorded by the emulator (files/ime-diag.txt).
        Button imeDiag = new Button(this);
        imeDiag.setText("입력 진단 기록 보기");
        imeDiag.setOnClickListener(v -> showImeDiag());
        layout.addView(imeDiag);

        Button runTest = new Button(this);
        runTest.setText("SDL 시험 화면 (ROM·세션 없이)");
        runTest.setOnClickListener(v -> startEmulator(false));
        layout.addView(runTest);

        setContentView(layout);
        updateStatus(null);
    }

    private void startEmulator(boolean emulate) {
        Intent intent = new Intent(this, EmulatorActivity.class);
        if (emulate) {
            intent.putExtra(EmulatorActivity.EXTRA_ROM, romFile().isFile() ? romFile().getAbsolutePath() : "");
            intent.putExtra(EmulatorActivity.EXTRA_SESSION, sessionFile().getAbsolutePath());
        }
        startActivity(intent);
    }

    private void showImeDiag() {
        File file = new File(getFilesDir(), "ime-diag.txt");
        String text;
        try (InputStream in = new java.io.FileInputStream(file)) {
            byte[] data = new byte[(int) Math.min(file.length(), 256 * 1024)];
            int n = 0, r;
            while (n < data.length && (r = in.read(data, n, data.length - n)) > 0) n += r;
            text = new String(data, 0, n, java.nio.charset.StandardCharsets.UTF_8);
        } catch (IOException e) {
            text = "기록 없음 (에뮬레이터에서 휴대폰 키보드를 쓰면 기록됩니다)";
        }

        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(11);
        view.setTypeface(android.graphics.Typeface.MONOSPACE);
        view.setTextIsSelectable(true);
        final int pad = (int) (8 * getResources().getDisplayMetrics().density);
        view.setPadding(pad, pad, pad, pad);
        android.widget.ScrollView scroll = new android.widget.ScrollView(this);
        scroll.addView(view);

        new AlertDialog.Builder(this)
                .setTitle("입력 진단 기록 (최근 60건)")
                .setView(scroll)
                .setPositiveButton("닫기", null)
                .setNeutralButton("지우기", (d, w) -> file.delete())
                .show();
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateStatus(null);  // the emulator may have saved or moved the session
    }

    private void updateStatus(String message) {
        File rom = romFile();
        File session = sessionFile();
        File bad = new File(session.getPath() + ".bad");
        DateFormat format = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT);

        StringBuilder text = new StringBuilder();
        if (message != null) text.append(message).append("\n");
        text.append(rom.isFile() ? "저장된 ROM: " + rom.length() + " 바이트" : "저장된 ROM 없음");
        text.append("\n").append(session.isFile()
                ? "저장된 세션: " + session.length() + " 바이트, " + format.format(new Date(session.lastModified()))
                : "저장된 세션 없음");
        if (bad.isFile()) text.append("\n손상된 세션을 tg50.ses.bad로 옮겨 두었습니다 (새로 부팅했습니다)");

        status.setText(text);
        run.setEnabled(rom.isFile() || session.isFile());
        deleteSession.setEnabled(session.isFile());
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_ROM || resultCode != RESULT_OK || data == null || data.getData() == null)
            return;

        final Uri uri = data.getData();
        status.setText("복사 중…");
        new Thread(() -> {
            String message;
            try {
                long size = copyRom(uri);
                message = "복사 완료: " + size + " 바이트";
                Log.i(TAG, "ROM copied, " + size + " bytes");
            } catch (IOException e) {
                message = "복사 실패: " + e.getMessage();
                Log.w(TAG, "ROM copy failed", e);
            }
            final String result = message;
            runOnUiThread(() -> updateStatus(result));
        }).start();
    }

    // Copy to a temporary file first so a failed copy never leaves a partial rom.bin.
    private long copyRom(Uri uri) throws IOException {
        File target = romFile();
        File temp = new File(getFilesDir(), "rom.bin.tmp");
        long total = 0;

        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(temp)) {
            if (in == null) throw new IOException("파일을 열 수 없습니다");
            byte[] buffer = new byte[65536];
            int n;
            while ((n = in.read(buffer)) > 0) {
                total += n;
                if (total > MAX_ROM_SIZE) throw new IOException("파일이 너무 큽니다 (64 MiB 초과)");
                out.write(buffer, 0, n);
            }
        } catch (IOException e) {
            temp.delete();
            throw e;
        }

        if (total == 0) {
            temp.delete();
            throw new IOException("빈 파일입니다");
        }
        if (!temp.renameTo(target)) {
            temp.delete();
            throw new IOException("저장에 실패했습니다");
        }
        return total;
    }
}
