package io.github.muns5965.cloudpilot;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Start screen (test build): pick a ROM with the system file picker, copy it to app storage, and
 * start the emulator. A saved TG50 session (files/tg50.ses, written by the emulator when it goes
 * to the background or quits) is resumed; without one a TG50 ROM boots cold. Anything else shows
 * the SDL test screen.
 *
 * PRC / PDB files picked here are copied to files/install/pending; the emulator installs them
 * once the Palm OS has started and records the results in files/install/status.txt (see
 * AndroidInstall.h), shown here.
 */
public class MainActivity extends Activity {
    private static final String TAG = "cp-android";
    private static final int PICK_ROM = 1;
    private static final int PICK_DB = 2;
    private static final long MAX_ROM_SIZE = 64L * 1024 * 1024;
    private static final long MAX_DB_SIZE = 16L * 1024 * 1024;  // as AndroidInstall.cpp
    private static final int STATUS_LINES = 8;

    private TextView status;
    private Button run;
    private Button deleteSession;

    private File romFile() {
        return new File(getFilesDir(), "rom.bin");
    }

    private File sessionFile() {
        return new File(getFilesDir(), "tg50.ses");
    }

    private File installDir() {
        return new File(getFilesDir(), "install");
    }

    private File pendingDir() {
        return new File(installDir(), "pending");
    }

    private File installStatusFile() {
        return new File(installDir(), "status.txt");
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
        title.setText("CloudpilotEmu Android 시험 빌드 (2-F)\nTG50 ROM만 부팅합니다. 백그라운드로 가거나 종료할 때 세션을 저장하고, 다음 실행 때 이어서 시작합니다. 추가한 PRC/PDB는 다음 실행 때 설치합니다.");
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

        Button addDb = new Button(this);
        addDb.setText("PRC/PDB 추가 (다음 실행 때 설치)");
        addDb.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
            startActivityForResult(intent, PICK_DB);
        });
        layout.addView(addDb);

        deleteSession = new Button(this);
        deleteSession.setText("저장된 세션 지우기");
        deleteSession.setOnClickListener(v -> new AlertDialog.Builder(this)
                .setMessage("저장된 세션(Palm 데이터·설정·설치한 프로그램)과 설치 기록을 지웁니다. 다음 실행은 ROM으로 처음부터 부팅합니다. 설치 대기 중인 파일은 남습니다.")
                .setPositiveButton("지우기", (d, w) -> {
                    boolean deleted = sessionFile().delete();
                    // The install results describe the deleted session.
                    installStatusFile().delete();
                    updateStatus(deleted ? "세션을 지웠습니다" : "세션을 지우지 못했습니다");
                })
                .setNegativeButton("취소", null)
                .show());
        layout.addView(deleteSession);

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
        appendInstallStatus(text);

        status.setText(text);
        run.setEnabled(rom.isFile() || session.isFile());
        deleteSession.setEnabled(session.isFile());
    }

    // ---- PRC / PDB installation queue (stage 2-F) ----------------------------------------------

    // Key: <time>-<index>-<name>, so the queue sorts in the order files were added.
    private static String displayName(String key) {
        String[] parts = key.split("-", 3);
        return parts.length == 3 ? parts[2] : key;
    }

    private static String describe(String state, String reason) {
        switch (state) {
            case "saved":
                if (reason.equals("reset")) return "설치 완료 (리셋함)";
                if (!reason.isEmpty()) return "설치 완료, 확인 필요 (Palm 데이터베이스 목록에 없음)";
                return "설치 완료";
            case "reset-needed":
                return "설치 완료, 리셋 필요";
            case "failed":
                return "실패: " + describeFailure(reason);
            case "deferred":
                return reason.equals("session not saved")
                        ? "세션을 저장하지 못함, 다음 실행 때 다시 설치"
                        : "지금은 설치할 수 없음, 다음 실행 때 다시 시도";
            default:  // pending, retrying, unsaved, reset-unsaved: the emulator is still at it
                return "설치 대기";
        }
    }

    private static String describeFailure(String reason) {
        switch (reason) {
            case "corrupt": return "손상되었거나 PRC/PDB가 아닌 파일";
            case "out of memory": return "Palm 메모리 부족";
            case "open": return "Palm에서 사용 중인 데이터베이스 (런처로 돌아간 뒤 다시 추가하세요)";
            case "could not overwrite": return "기존 데이터베이스를 덮어쓸 수 없음";
            case "not supported": return "이 ROM에서는 설치할 수 없음";
            case "too large": return "파일이 너무 큼";
            case "cannot read": return "파일을 읽을 수 없음";
            default: return "알 수 없는 오류";
        }
    }

    // The last STATUS_LINES entries: results recorded by the emulator, then files still queued.
    private void appendInstallStatus(StringBuilder text) {
        Map<String, String> lines = new LinkedHashMap<>();

        File statusFile = installStatusFile();
        if (statusFile.isFile()) {
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(new FileInputStream(statusFile), StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    if (line.startsWith("#")) continue;
                    String[] f = line.split("\t", -1);  // key, state, time, db name, reason
                    if (f.length != 5) continue;
                    String name = displayName(f[0]);
                    if (!f[3].isEmpty() && !f[1].equals("failed")) name += " (" + f[3] + ")";
                    lines.put(f[0], name + ": " + describe(f[1], f[4]));
                }
            } catch (IOException e) {
                Log.w(TAG, "cannot read install status", e);
            }
        }

        String[] pending = pendingDir().list();
        if (pending != null) {
            Arrays.sort(pending);
            for (String key : pending) {
                if (key.endsWith(".tmp") || lines.containsKey(key)) continue;
                lines.put(key, displayName(key) + ": 설치 대기");
            }
        }

        if (lines.isEmpty()) return;
        List<String> all = new ArrayList<>(lines.values());
        text.append("\n\nPRC/PDB 설치");
        for (String line : all.subList(Math.max(0, all.size() - STATUS_LINES), all.size()))
            text.append("\n· ").append(line);
    }

    private String queryName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(
                uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) return cursor.getString(0);
        } catch (RuntimeException e) {
            Log.w(TAG, "cannot query file name", e);
        }
        String path = uri.getLastPathSegment();
        return path == null ? "file" : path.substring(path.lastIndexOf('/') + 1);
    }

    // Copies picked files into the queue (a temporary file first, renamed when complete).
    private String queueDatabases(List<Uri> uris) {
        File dir = pendingDir();
        if (!dir.isDirectory() && !dir.mkdirs()) return "설치 폴더를 만들 수 없습니다";

        long time = System.currentTimeMillis();
        int added = 0;
        StringBuilder skipped = new StringBuilder();

        for (int i = 0; i < uris.size(); i++) {
            String name = queryName(uris.get(i));
            String lower = name.toLowerCase(Locale.ROOT);
            if (!lower.endsWith(".prc") && !lower.endsWith(".pdb") && !lower.endsWith(".pqa")) {
                skipped.append("\n").append(name).append(": PRC/PDB/PQA 파일이 아님");
                continue;
            }

            // File name characters only (no path separators or control characters).
            String safe = name.replaceAll("[^\\p{L}\\p{N}._ ()\\[\\]-]", "_");
            String key = String.format(Locale.ROOT, "%013d-%02d-%s", time, i, safe);
            File target = new File(dir, key);
            File temp = new File(dir, key + ".tmp");

            try {
                long size = copy(uris.get(i), temp, MAX_DB_SIZE, "16 MiB");
                if (size == 0) throw new IOException("빈 파일입니다");
                if (!temp.renameTo(target)) throw new IOException("저장에 실패했습니다");
                added++;
                Log.i(TAG, "queued " + key + ", " + size + " bytes");
            } catch (IOException e) {
                temp.delete();
                skipped.append("\n").append(name).append(": ").append(e.getMessage());
                Log.w(TAG, "queueing " + name + " failed", e);
            }
        }

        String message = added + "개를 추가했습니다. 다음 실행 때 설치합니다.";
        if (skipped.length() > 0) message += "\n추가하지 않은 파일:" + skipped;
        return message;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_DB) {
            if (resultCode != RESULT_OK || data == null) return;

            final List<Uri> uris = new ArrayList<>();
            ClipData clip = data.getClipData();
            if (clip != null) {
                for (int i = 0; i < clip.getItemCount(); i++) uris.add(clip.getItemAt(i).getUri());
            } else if (data.getData() != null) {
                uris.add(data.getData());
            }
            if (uris.isEmpty()) return;

            status.setText("복사 중…");
            new Thread(() -> {
                final String result = queueDatabases(uris);
                runOnUiThread(() -> updateStatus(result));
            }).start();
            return;
        }

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

    // Copies uri to file, at most maxSize bytes; on failure the file is deleted.
    private long copy(Uri uri, File file, long maxSize, String maxText) throws IOException {
        long total = 0;
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(file)) {
            if (in == null) throw new IOException("파일을 열 수 없습니다");
            byte[] buffer = new byte[65536];
            int n;
            while ((n = in.read(buffer)) > 0) {
                total += n;
                if (total > maxSize) throw new IOException("파일이 너무 큽니다 (" + maxText + " 초과)");
                out.write(buffer, 0, n);
            }
        } catch (IOException e) {
            file.delete();
            throw e;
        }
        return total;
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
