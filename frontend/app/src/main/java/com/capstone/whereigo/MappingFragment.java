package com.capstone.whereigo;

import android.animation.ObjectAnimator;
import android.hardware.display.DisplayManager;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.transition.AutoTransition;
import androidx.transition.Transition;
import androidx.transition.TransitionManager;

import com.capstone.whereigo.databinding.FragmentMappingBinding;

import java.util.List;
import java.util.Locale;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MappingFragment extends Fragment implements GLSurfaceView.Renderer, DisplayManager.DisplayListener {
    private final String TAG = "MappingFragment";

    private PoseStampViewModel viewModel;
    private PoseStampRecyclerViewAdapter poseStampRecyclerViewAdapter;
    private FragmentMappingBinding binding;
    private ConstraintLayout main_layout;

    private boolean isScaledDown = false;

    private TextView tvNumberOfRecordedNode;
    private Button btnStartSavePoseGraph, btnPoseStamp;
    private GLSurfaceView surfaceView;

    private long nativeApplication;

    private int viewportWidth;
    private int viewportHeight;
    private boolean viewportChanged = false;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater,
                             @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        binding = FragmentMappingBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

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

        JniInterface.setClassLoader(this.getClass().getClassLoader());
        JniInterface.assetManager = requireActivity().getAssets();

        nativeApplication = JniInterface.createNativeApplication(
                JniInterface.assetManager,
                requireActivity().getExternalFilesDir("pose_graph").getAbsolutePath()
        );

        registerNativeSelf(nativeApplication);

        poseStampRecyclerViewAdapter = new PoseStampRecyclerViewAdapter();
        RecyclerView recyclerView = binding.recyclerView;
        recyclerView.setAdapter(poseStampRecyclerViewAdapter);
        recyclerView.setLayoutManager(new LinearLayoutManager(requireContext()));
        recyclerView.addItemDecoration(new DividerItemDecoration(requireContext(), LinearLayoutManager.VERTICAL));

        viewModel = new ViewModelProvider(requireActivity()).get(PoseStampViewModel.class);
        viewModel.getPoseStampList().observe(getViewLifecycleOwner(), poseStampList -> {
            if(!poseStampList.isEmpty()){
                PoseStamp last = poseStampList.get(poseStampList.size() - 1);
                poseStampRecyclerViewAdapter.addPoseStamp(last);
                recyclerView.post(() -> {
                    recyclerView.scrollToPosition(poseStampRecyclerViewAdapter.getItemCount() - 1);
                });
            }

            else {
                poseStampRecyclerViewAdapter.clearPoseStampList();
            }
        });

        tvNumberOfRecordedNode = binding.numberOfRecordedNode;

        main_layout = binding.buttonGroup;

        btnStartSavePoseGraph = binding.buttonSavePosegraph;
        btnStartSavePoseGraph.setOnClickListener(this::toggleScaleListener);

        btnPoseStamp = binding.buttonPoseStamp;
        btnPoseStamp.setOnClickListener(v -> {
            JniInterface.getPoseStamp(nativeApplication);
            float x = JniInterface.getX();
            float z = JniInterface.getZ();

            PoseStamp newPoseStamp = new PoseStamp(x, z, R.drawable.test);
            viewModel.addPoseStamp(newPoseStamp);
        });
    }

    public native void registerNativeSelf(long nativeApplicationPtr);

    private void toggleScaleListener(View v) {
        if(!isScaledDown){
            isScaledDown = true;

            JniInterface.changeStatus(nativeApplication);
            btnStartSavePoseGraph.setText("저장하기");
            animateConstraintLayout();
            fadeBtnPoseStamp();
        }

        else if(/* isScaledDown && */ viewModel.getPoseStampListSize() > 0){
            isScaledDown = false;

            btnStartSavePoseGraph.setText("지도 작성 하기");
            animateConstraintLayout();
            fadeBtnPoseStamp();

            JniInterface.changeStatus(nativeApplication);

            PoseStampLabelingDialog dialog = PoseStampLabelingDialog.newInstance(viewModel);
            dialog.onDismissListener = () -> {
                List<String> data = viewModel.getPoseStampLabelList().getValue();
                data.add(String.format(Locale.ROOT, "%s/%s", viewModel.getBuildingName(), viewModel.getFloorName()));

                JniInterface.savePoseGraph(
                        nativeApplication,
                        data.toArray(new String[0]));

                viewModel.clearPoseStampList();

                requireActivity().getSupportFragmentManager().beginTransaction()
                        .replace(R.id.layout_mapping_main, BuildingInputFragment.newInstance(true))
                        .commit();
            };
            
            dialog.show(requireActivity().getSupportFragmentManager(), "nodeLabelDialog");
        }

        else /* isScaledDown && !indexList.isEmpty() */ {
            Toast.makeText(requireContext(), "Stamp가 찍히지 않았습니다.", Toast.LENGTH_SHORT).show();
        }
    }

    public void updateKeyFrameListSize(int size){
        tvNumberOfRecordedNode.post(() -> {
            tvNumberOfRecordedNode.setText("지금까지 기록된 노드 수 : " + size);
        });
    }

    private void animateConstraintLayout(){
        if(isScaledDown){
            btnPoseStamp.setAlpha(0f);
            btnPoseStamp.setVisibility(View.VISIBLE);
        }

        else {
            btnPoseStamp.setVisibility(View.GONE);
        }

        Transition transition = new AutoTransition();
        transition.setDuration(500);

        final ConstraintSet constraintSet = new ConstraintSet();
        constraintSet.clone(main_layout);

        constraintSet.clear(btnStartSavePoseGraph.getId(), ConstraintSet.END);
        if(isScaledDown){
            constraintSet.connect(btnStartSavePoseGraph.getId(), ConstraintSet.END,
                    binding.buttonGroupGuideLineLeft.getId(), ConstraintSet.END, 0);

        }

        else {
            constraintSet.connect(btnStartSavePoseGraph.getId(), ConstraintSet.END,
                    ConstraintSet.PARENT_ID, ConstraintSet.END, 0);
        }

        TransitionManager.beginDelayedTransition(main_layout, transition);
        constraintSet.applyTo(main_layout);
    }

    private void fadeBtnPoseStamp() {
        if (isScaledDown) {
            btnPoseStamp.setAlpha(0f);
            btnPoseStamp.setVisibility(View.VISIBLE);
        }

        final float startAlpha = isScaledDown ? 0f : 1f;
        final float endAlpha = isScaledDown ? 1f : 0f;

        ObjectAnimator fade = ObjectAnimator.ofFloat(btnPoseStamp, "alpha", startAlpha, endAlpha);
        fade.setDuration(500);
        fade.start();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (!CameraPermissionHelper.hasCameraPermission(requireActivity())) {
            CameraPermissionHelper.requestCameraPermission(requireActivity());
            return;
        }

        try {
            JniInterface.onResume(nativeApplication, requireContext(), requireActivity());
            surfaceView.onResume();
        } catch (Exception e) {
            Log.e(TAG, "Exception creating session", e);
            return;
        }

        requireContext().getSystemService(DisplayManager.class).registerDisplayListener(this, null);
    }

    @Override
    public void onPause() {
        super.onPause();
        surfaceView.onPause();
        JniInterface.onPause(nativeApplication);
        requireContext().getSystemService(DisplayManager.class).unregisterDisplayListener(this);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        new Thread(() -> {
            synchronized (this) {
                JniInterface.destroyNativeApplication(nativeApplication);
                nativeApplication = 0;
            }
        }).start();
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

    @Override public void onDrawFrame(GL10 gl) {
        synchronized (this) {
            if (nativeApplication == 0) return;
            if (viewportChanged) {
                int displayRotation = requireActivity().getWindowManager().getDefaultDisplay().getRotation();
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
        binding = null;
    }
}
