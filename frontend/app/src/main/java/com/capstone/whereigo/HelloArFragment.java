package com.capstone.whereigo;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Color;
import android.hardware.display.DisplayManager;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.speech.SpeechRecognizer;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.capstone.whereigo.databinding.FragmentHelloArBinding;
import com.google.android.material.search.SearchBar;
import com.google.android.material.search.SearchView;
import com.google.android.material.snackbar.Snackbar;
import com.capstone.whereigo.ui.DirectionCompassView;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.media.MediaPlayer;
import android.net.Uri;
import android.content.res.AssetFileDescriptor;
import java.io.IOException;
import android.content.Context;
import java.util.Queue;
import java.util.LinkedList;
import android.content.res.AssetManager;
import android.os.Vibrator;
import android.os.VibrationEffect;
import android.os.Build;


public class HelloArFragment extends Fragment {
  private static final String TAG = "HelloArFragment";
  private static final int SNACKBAR_UPDATE_INTERVAL_MILLIS = 1000;

  private final int RECORD_AUDIO_REQUEST_CODE = 100;
  private long backPressedTime = 0;
  private final long backPressInterval = 1000;

  private DirectionCompassView compassView;

  private GestureDetector gestureDetector;

  private FragmentActivity activity;

  private static HelloArFragment instance;

  private static MediaPlayer player = null;

  private static Queue<String> audioQueue = new LinkedList<>();
  private static boolean isPlaying = false;

  private FragmentHelloArBinding binding;

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    activity = requireActivity();

    binding = FragmentHelloArBinding.bind(
            inflater.inflate(R.layout.fragment_hello_ar, container, false)
    );

    SearchBar searchBar = binding.searchBar;
    SearchView searchView = binding.searchView;
    searchView.setupWithSearchBar(searchBar);

    int searchMenu = R.menu.search_menu;
    searchBar.inflateMenu(searchMenu);

    ImageButton settingsButton = binding.settingsButton;
    settingsButton.setOnClickListener(v -> {
      Intent intent = new Intent(requireContext(), SettingActivity.class);
      startActivity(intent);
      requireActivity().finish();
    });

    searchBar.getMenu().findItem(R.id.action_voice_search).setOnMenuItemClickListener(item -> {
      if (checkAudioPermission()) {
        if (SpeechRecognizer.isRecognitionAvailable(activity)) {
          VoiceRecordDialog dialog = new VoiceRecordDialog();
          dialog.show(activity.getSupportFragmentManager(), "VoiceRecordDialog");
        } else {
          Toast.makeText(activity, "음성 인식을 사용할 수 없습니다.", Toast.LENGTH_SHORT).show();
        }
      } else {
        requestAudioPermission();
      }
      return true;
    });

    RecyclerView recyclerView = searchView.findViewById(R.id.search_result);

    List<String> allResults = new ArrayList<>();
    allResults.add("미래관 445호");
    allResults.add("미래관 447호");
    allResults.add("미래관 449호");
    allResults.add("미래관 444호");
    allResults.add("미래관 425호");
    allResults.add("미래관 415호");
    allResults.add("미래관 405호");

    searchView.getEditText().addTextChangedListener(new TextWatcher() {
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        // 필요 없다면 비워둠
      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {
        String query = s.toString().trim();
        List<String> filtered = new ArrayList<>();
        for (String item : allResults) {
          if (item.toLowerCase().contains(query.toLowerCase())) {
            filtered.add(item);
          }
        }

        if (!query.isEmpty()) {
          recyclerView.setVisibility(View.VISIBLE);
          recyclerView.setLayoutManager(new LinearLayoutManager(activity));
          recyclerView.setAdapter(new SearchResultAdapter(filtered, selected -> {
            // 예: "미래관 445호" → "미래관"
            String buildingName = selected.split(" ")[0];
            String fileName = buildingName + ".zip";

            String url = "https://media-server-jubin.s3.amazonaws.com/" + buildingName + "/" + fileName;
            Log.d("filename", url);
            FileDownloader.downloadAndUnzipFile(activity, url, fileName, buildingName);
          }));
        } else {
          recyclerView.setVisibility(View.GONE);
        }
      }

      @Override
      public void afterTextChanged(Editable s) {
        // 필요 없다면 비워둠
      }
    })

    return binding.getRoot();
  }

  @SuppressLint("ClickableViewAccessibility")
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    TtsManager.INSTANCE.init(requireContext());
    AudioManager.getInstance().init(requireContext());

    compassView = view.findViewById(R.id.compassView);
    setupWakeWordListener();
  }

  public static void updateYawFromNative(float cameraYaw, float pathYaw) {
    if (instance != null && instance.compassView != null) {
//      Log.d("HelloArFragment", "updateYawFromNative called: cameraYaw=" + cameraYaw + ", pathYaw=" + pathYaw);
      instance.compassView.post(() -> instance.compassView.setYawValues(cameraYaw, pathYaw));
    }
  }

  @Override
  public void onDestroyView() {
    super.onDestroyView();
    binding = null;
  }

  public void onAttach(@NonNull Context context) {
    super.onAttach(context);
    instance = this;
  }

  public static void vibrateOnce() {
    Vibrator vibrator = (Vibrator) instance.requireContext().getSystemService(Context.VIBRATOR_SERVICE);
    if (vibrator != null && vibrator.hasVibrator()) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createOneShot(300, VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            vibrator.vibrate(300); // deprecated but for older versions
        }
    }
  }


  private boolean checkAudioPermission() {
    return ContextCompat.checkSelfPermission(
            activity,
            android.Manifest.permission.RECORD_AUDIO
    ) == PackageManager.PERMISSION_GRANTED;
  }

  private void requestAudioPermission() {
    ActivityCompat.requestPermissions(
            activity,
            new String[]{Manifest.permission.RECORD_AUDIO},
            RECORD_AUDIO_REQUEST_CODE
    );
  }

  @Override
  public void onRequestPermissionsResult(int requestCode,
                                         @NonNull String[] permissions,
                                         @NonNull int[] grantResults) {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);

    if (requestCode == RECORD_AUDIO_REQUEST_CODE) {
      if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
        Toast.makeText(activity, "음성 권한이 허용되었습니다. 다시 시도해주세요.", Toast.LENGTH_SHORT).show();
      } else {
        Toast.makeText(activity, "음성 권한이 거부되었습니다.", Toast.LENGTH_SHORT).show();
      }
    }
  }

  private void setupWakeWordListener() {
    if (!checkAudioPermission()) {
      requestAudioPermission();
    }
  }
}
