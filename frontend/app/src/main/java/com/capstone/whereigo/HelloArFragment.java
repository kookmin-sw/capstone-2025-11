package com.capstone.whereigo;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.PackageManager;
import android.hardware.display.DisplayManager;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.speech.SpeechRecognizer;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.capstone.whereigo.databinding.FragmentHelloArBinding;
import com.google.android.material.search.SearchBar;
import com.google.android.material.search.SearchView;
import com.capstone.whereigo.ui.DirectionCompassView;

import java.util.ArrayList;
import java.util.List;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.media.MediaPlayer;
import android.content.Context;

import java.util.Queue;
import java.util.LinkedList;
import android.os.Vibrator;
import android.os.VibrationEffect;
import android.os.Build;

public class HelloArFragment extends Fragment implements GLSurfaceView.Renderer, DisplayManager.DisplayListener {
  private static final String TAG = "HelloArFragment";

  private GLSurfaceView surfaceView;
  private boolean viewportChanged = false;
  private int viewportWidth;
  private int viewportHeight;

  private Handler planeStatusCheckingHandler;
  private Runnable planeStatusCheckingRunnable;

  private long nativeApplication;
  private FragmentActivity activity;

  private static HelloArFragment instance;

  private static DirectionCompassView compassView;
  private static ImageButton elevatorButton;
  private static TextView tvElevator;

  private FragmentHelloArBinding binding;

  private int currentFloor;
  private String buildingName, roomName;

  public HelloArFragment(String buildingName, String roomName, int currentFloor){
    this.buildingName = buildingName;
    this.roomName = roomName;
    this.currentFloor = currentFloor;
  }

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    activity = requireActivity();

    binding = FragmentHelloArBinding.bind(
            inflater.inflate(R.layout.fragment_hello_ar, container, false)
    );

    planeStatusCheckingHandler = new Handler();

    return binding.getRoot();
  }

  @SuppressLint("ClickableViewAccessibility")
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    surfaceView = binding.surfaceview;
    surfaceView.setPreserveEGLContextOnPause(true);
    surfaceView.setEGLContextClientVersion(2);
    surfaceView.setEGLConfigChooser(8, 8, 8, 8, 16, 0);
    surfaceView.setRenderer(this);
    surfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
    surfaceView.setWillNotDraw(false);

    JniInterface.assetManager = activity.getAssets();
    nativeApplication = JniInterface.createNativeApplication(activity.getAssets(), this.getContext().getExternalFilesDir("pose_graph").getAbsolutePath(), true);

    JniInterface.setClassLoader(this.getClass().getClassLoader());
    TtsManager.INSTANCE.init(requireContext());
    AudioManager.getInstance().init(requireContext());

    elevatorButton = binding.btnElevator;
    tvElevator = binding.tvElevator;
    compassView = binding.compassView;

    SearchBar searchBarArrive = requireActivity().findViewById(R.id.search_bar_arrive);
    searchBarArrive.setText("도착지 : " + roomName);

    SearchBar searchBarDeparture = requireActivity().findViewById(R.id.search_bar_departure);
    searchBarDeparture.setText("출발지 : 본부관 1층");

    SearchResultHandler.handle(
            requireContext(),
            roomName,
            this.buildingName,
            () -> (HelloArFragment) requireActivity().getSupportFragmentManager().findFragmentById(R.id.path_navigation),
            this.currentFloor
    );
  }

  public void sendMultiGoalsToNative(float[] coords) {
    if (nativeApplication != 0 && coords != null && coords.length % 2 == 0) {
      Log.i("HelloArFragment", "📤 sendMultiGoalsToNative: 총 " + (coords.length / 2) + "개 좌표 전송");

      JniInterface.sendMultiGoalsToNative(nativeApplication, coords);
    } else {
      Log.e("HelloArFragment", "❌ nativeApplication 또는 coords 오류");
    }
  }

  public void loadPoseGraphFromFile(String filePath, int floor) {
    if (nativeApplication != 0) {
      JniInterface.loadPoseGraphFromFile(nativeApplication, filePath, floor);
    }
  }

  public void setCurrentFloor(int floor) {
    JniInterface.setCurrentFloor(nativeApplication, floor);
  }

  public static void updateYawFromNative(float cameraYaw, float pathYaw) {
    if (instance != null && instance.compassView != null) {
//      Log.d("HelloArFragment", "updateYawFromNative called: cameraYaw=" + cameraYaw + ", pathYaw=" + pathYaw);
      instance.compassView.post(() -> instance.compassView.setYawValues(cameraYaw, pathYaw));
    }
  }

  @Override public void onSurfaceCreated(GL10 gl, EGLConfig config) {
    GLES20.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    JniInterface.onGlSurfaceCreated(nativeApplication);
  }

  @Override public void onSurfaceChanged(GL10 gl, int width, int height) {
    viewportWidth = width;
    viewportHeight = height;
    viewportChanged = true;
  }

  @Override
  public void onResume() {
    super.onResume();
    if (!CameraPermissionHelper.hasCameraPermission(activity)) {
      CameraPermissionHelper.requestCameraPermission(activity);
      return;
    }

    try {
      JniInterface.onResume(nativeApplication, activity.getApplicationContext(), activity);
      surfaceView.onResume();
    } catch (Exception e) {
      Log.e(TAG, "Exception creating session", e);
      return;
    }

    activity.getSystemService(DisplayManager.class).registerDisplayListener(this, null);
  }

  @Override
  public void onPause() {
    super.onPause();
    surfaceView.onPause();
    JniInterface.onPause(nativeApplication);
    if (planeStatusCheckingHandler != null && planeStatusCheckingRunnable != null) {
      planeStatusCheckingHandler.removeCallbacks(planeStatusCheckingRunnable);
    }
    activity.getSystemService(DisplayManager.class).unregisterDisplayListener(this);
  }

  @Override public void onDrawFrame(GL10 gl) {
    synchronized (this) {
      if (nativeApplication == 0) return;
      if (viewportChanged) {
        int displayRotation = activity.getWindowManager().getDefaultDisplay().getRotation();
        JniInterface.onDisplayGeometryChanged(nativeApplication, displayRotation, viewportWidth, viewportHeight);
        viewportChanged = false;
      }
      JniInterface.onGlSurfaceDrawFrame(nativeApplication, false, false);
    }
  }

  @Override public void onDisplayAdded(int displayId) {}
  @Override public void onDisplayRemoved(int displayId) {}
  @Override public void onDisplayChanged(int displayId) {
    viewportChanged = true;
  }

  @Override
  public void onDestroyView() {
    super.onDestroyView();
    synchronized (this) {
      JniInterface.destroyNativeApplication(nativeApplication);
      nativeApplication = 0;
    }
    binding = null;
  }

  public static void onGoalStatusChanged(int status) {
    if (instance == null) return;

    String message = (status == 0)
            ? "다음 목표로 이동합니다"
            : "목적지에 도착했습니다";

    instance.requireActivity().runOnUiThread(() -> {
      Toast.makeText(instance.requireContext(), message, Toast.LENGTH_SHORT).show();

      if (elevatorButton != null) {
        if (status == 0) {
          compassView.setVisibility(View.GONE);
          elevatorButton.setVisibility(View.VISIBLE);
          tvElevator.setVisibility(View.VISIBLE);

          elevatorButton.setOnClickListener(v -> {
            JniInterface.restartSession(instance.nativeApplication, instance.activity.getApplicationContext(), instance.activity);
            JniInterface.changeStatus(instance.nativeApplication);
            JniInterface.setCurrentFloor(instance.nativeApplication, SearchResultHandler.goal_floor);
            elevatorButton.setVisibility(View.GONE);
            tvElevator.setVisibility(View.GONE);
            compassView.setVisibility(View.VISIBLE);
          });

        } else {
          elevatorButton.setVisibility(View.GONE);
          tvElevator.setVisibility(View.GONE);
        }
      }
    });

    Log.d("NativeCallback", "🎯 onGoalStatusChanged: " + message);
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
}
