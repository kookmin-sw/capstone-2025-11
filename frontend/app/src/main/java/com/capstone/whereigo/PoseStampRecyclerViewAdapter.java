package com.capstone.whereigo;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.capstone.whereigo.databinding.ItemPoseStampDataBinding;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class PoseStampRecyclerViewAdapter extends RecyclerView.Adapter<PoseStampRecyclerViewAdapter.PoseStampViewHolder> {
    private final List<PoseStamp> poseStampList = new ArrayList<>();

    public void addPoseStamp(PoseStamp newPoseStamp) {
        poseStampList.add(newPoseStamp);
        notifyItemInserted(poseStampList.size() - 1);
    }

    public void clearPoseStampList() {
        poseStampList.clear();
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public PoseStampViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());
        ItemPoseStampDataBinding binding = ItemPoseStampDataBinding.inflate(inflater, parent, false);
        return new PoseStampViewHolder(binding);
    }

    @Override
    public void onBindViewHolder(@NonNull PoseStampViewHolder holder, int position) {
        holder.binding.tvPoseStamp.setText(
                String.format(
                        Locale.ROOT,
                        "x: %f z: %f",
                        poseStampList.get(position).x(),
                        poseStampList.get(position).z()
                )
        );
    }

    @Override
    public int getItemCount() {
        return poseStampList.size();
    }

    public static class PoseStampViewHolder extends RecyclerView.ViewHolder {
        public final ItemPoseStampDataBinding binding;

        public PoseStampViewHolder(ItemPoseStampDataBinding binding) {
            super(binding.getRoot());
            this.binding = binding;
        }
    }
}
