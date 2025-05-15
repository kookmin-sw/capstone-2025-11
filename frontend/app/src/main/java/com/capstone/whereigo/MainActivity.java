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
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.Toast;
import android.content.Intent;

import androidx.appcompat.app.AlertDialog;
import androidx.core.view.WindowCompat;


import androidx.activity.OnBackPressedCallback;
import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.capstone.whereigo.databinding.ActivityMainBinding;
import com.google.android.material.search.SearchBar;
import com.google.android.material.search.SearchView;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        EdgeToEdge.enable(this);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        ImageButton backButton = findViewById(R.id.back_button);
        backButton.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, SearchActivity.class);
            startActivity(intent);
            finish();
        });

        ImageButton settingsButton = findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, SettingActivity.class);
            startActivity(intent);
            finish();
        });

        if (savedInstanceState == null) {
            showFloorInputDialog();
        }
    }

    private void showFloorInputDialog() {
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_floor_input, null);
        EditText editText = dialogView.findViewById(R.id.edit_floor_input);

        new AlertDialog.Builder(this)
                .setTitle("현재 층 입력")
                .setView(dialogView)
                .setCancelable(false)
                .setPositiveButton("확인", (dialog, which) -> {
                    String input = editText.getText().toString().trim();
                    if (!input.isEmpty()) {
                        Toast.makeText(this, input + "층 선택됨", Toast.LENGTH_SHORT).show();

                        // HelloArFragment 로드
                        getSupportFragmentManager().beginTransaction()
                                .replace(R.id.fragment_container, new HelloArFragment())
                                .commit();
                    } else {
                        Toast.makeText(this, "층을 입력해주세요", Toast.LENGTH_SHORT).show();
                        showFloorInputDialog(); // 재실행
                    }
                })
                .show();
    }
}
