package io.github.muns5965.cloudpilot;

import android.app.Activity;
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

/**
 * Start screen (stage 2-B test build): pick a ROM with the system file picker, copy it to app
 * storage, and start the emulator with it (a TG50 ROM boots; anything else shows the SDL test
 * screen) or the test screen without a ROM.
 */
public class MainActivity extends Activity {
    private static final String TAG = "cp-android";
    private static final int PICK_ROM = 1;
    private static final long MAX_ROM_SIZE = 64L * 1024 * 1024;

    private TextView status;
    private Button runWithRom;

    private File romFile() {
        return new File(getFilesDir(), "rom.bin");
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
        title.setText("CloudpilotEmu Android 시험 빌드 (2-C)\nTG50 ROM만 부팅합니다. 실행할 때마다 처음부터 부팅하며 저장되지 않습니다.");
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

        Button runTest = new Button(this);
        runTest.setText("SDL 시험 화면 (ROM 없이)");
        runTest.setOnClickListener(v -> startEmulator(null));
        layout.addView(runTest);

        runWithRom = new Button(this);
        runWithRom.setText("선택한 ROM 실행 (TG50이면 부팅)");
        runWithRom.setOnClickListener(v -> startEmulator(romFile().getAbsolutePath()));
        layout.addView(runWithRom);

        setContentView(layout);
        updateStatus(null);
    }

    private void startEmulator(String romPath) {
        Intent intent = new Intent(this, EmulatorActivity.class);
        if (romPath != null) intent.putExtra(EmulatorActivity.EXTRA_ROM, romPath);
        startActivity(intent);
    }

    private void updateStatus(String message) {
        File rom = romFile();
        String text = rom.isFile() ? "저장된 ROM: " + rom.length() + " 바이트" : "저장된 ROM 없음";
        if (message != null) text = message + "\n" + text;
        status.setText(text);
        runWithRom.setEnabled(rom.isFile());
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
