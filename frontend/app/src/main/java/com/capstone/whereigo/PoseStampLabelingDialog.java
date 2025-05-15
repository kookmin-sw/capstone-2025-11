package com.capstone.whereigo;

import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.viewpager2.widget.MarginPageTransformer;
import androidx.viewpager2.widget.ViewPager2;

import com.capstone.whereigo.databinding.FragmentNodeLabelingBinding;

import java.util.regex.Pattern;

public class PoseStampLabelingDialog extends DialogFragment implements DialogInterface.OnDismissListener {
    private static final Pattern VALID_FILENAME_PATTERN = Pattern.compile("^[가-힣a-zA-Z0-9\\- _]+$");
    private FragmentNodeLabelingBinding binding;
    private PoseStampViewPagerAdapter adapter;
    private PoseStampViewModel viewModel;

    @FunctionalInterface
    public interface OnDismissListener {
        void OnDismiss();
    }

    public OnDismissListener onDismissListener;

    public static PoseStampLabelingDialog newInstance(PoseStampViewModel viewModel) {
        PoseStampLabelingDialog fragment = new PoseStampLabelingDialog();
        fragment.viewModel = viewModel;
        return fragment;
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {

        binding = FragmentNodeLabelingBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        ViewPager2 viewPager = binding.dialogViewPager;

        adapter = new PoseStampViewPagerAdapter(requireActivity(), viewModel);
        viewPager.setAdapter(adapter);
        viewPager.setPageTransformer(new MarginPageTransformer(30));
        viewPager.setCurrentItem(0, false);

        binding.buttonLabelingDone.setOnClickListener(v -> {
            for(int pos = 0; pos < adapter.getItemCount(); pos++){
                if(!checkBuildingName(viewModel.getLabelAt(pos))){
                    viewPager.setCurrentItem(pos, true);
                    Toast.makeText(requireContext(), "노드명에 띄어쓰기는 허용되지 않습니다.", Toast.LENGTH_SHORT).show();
                    return;
                }
            }

            int backStackCount = requireActivity().getSupportFragmentManager().getBackStackEntryCount();
            Toast.makeText(requireContext(), "저장이완료됐어용가리치킨더조이 : " + backStackCount, Toast.LENGTH_SHORT).show();
            dismiss();
        });
    }

    @Override
    public void onDismiss(@NonNull DialogInterface dialog) {
        super.onDismiss(dialog);
        if (onDismissListener != null) {
            onDismissListener.OnDismiss();
        }
    }

    @Override
    public void onStart() {
        super.onStart();

        if (getDialog() != null && getDialog().getWindow() != null) {
            getDialog().getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
            getDialog().getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }
    }

    private boolean checkBuildingName(String label) {
        return VALID_FILENAME_PATTERN.matcher(label).matches();
    }
}
