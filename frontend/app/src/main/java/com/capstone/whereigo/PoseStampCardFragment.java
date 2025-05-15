package com.capstone.whereigo;


import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import com.capstone.whereigo.databinding.CardNodeLabelingBinding;
import com.google.ar.core.Pose;

import java.util.Locale;

public class PoseStampCardFragment extends Fragment {
    private static final String ARG_LABEL = "label";
    private static final String ARG_POSITION = "position";
    private static final String ARG_IMAGE_RES_ID = "image_res_id";

    private CardNodeLabelingBinding binding;
    public interface CardInputListener {
        void onTextChanged(String newText);
    }

    public CardInputListener cardInputListener;
    public static PoseStampCardFragment newInstance(PoseStamp poseStamp, String label) {
        PoseStampCardFragment fragment = new PoseStampCardFragment();

        Bundle args = new Bundle();

        args.putString(ARG_LABEL, label);
        args.putString(ARG_POSITION, poseStamp == null ? "-" :
                String.format(
                        Locale.ROOT,
                        "x: %f y: %f",
                        poseStamp.x(), poseStamp.z()));
        args.putInt(ARG_IMAGE_RES_ID, poseStamp == null ? R.drawable.test : poseStamp.imageResId());

        fragment.setArguments(args);

        return fragment;
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {

        binding = CardNodeLabelingBinding.inflate(inflater, container, false);

        // 데이터 받기
        assert getArguments() != null;
        String label = getArguments().getString(ARG_LABEL);
        String position = getArguments().getString(ARG_POSITION);
        int imageResId = getArguments().getInt(ARG_IMAGE_RES_ID);

        EditText editLabel = binding.editStamp;

        if (position.charAt(0) != '-'){
            ImageView thumbnail = binding.thumbnail;
            thumbnail.setImageResource(imageResId);

            TextView positionText = binding.poseStamp;
            positionText.setText("위치 : " + position);
            editLabel.setText(label);
        }

        else {
            binding.thumbnail.setVisibility(View.GONE);
            binding.poseStamp.setVisibility(View.GONE);
            editLabel.setHint(label);
        }

        editLabel.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

             @Override
             public void afterTextChanged(Editable s) {
                 if(cardInputListener != null){
                     cardInputListener.onTextChanged(s.toString());
                 }
             }
        });

        return binding.getRoot();
    }
}
