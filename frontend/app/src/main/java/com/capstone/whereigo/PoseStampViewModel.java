package com.capstone.whereigo;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

public class PoseStampViewModel extends ViewModel {
    private final MutableLiveData<List<PoseStamp>> poseStampList = new MutableLiveData<>();
    private final MutableLiveData<List<String>> poseStampLabelList = new MutableLiveData<>();

    private String buildingName = "";

    private int floorMinIdx = 0;
    private final String[] floorMinItem = {
            "선택 안함", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "B10"
    };

    private int floorMaxIdx = 0;
    private final String[] floorMaxItem = {
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
            "11", "12", "13", "14", "15", "16", "17", "18", "19", "20",
            "21", "22", "23", "24", "25", "26", "27", "28", "29", "30",
            "31", "32", "33", "34", "35", "36", "37", "38", "39", "40",
            "41", "42", "43", "44", "45", "46", "47", "48", "49", "50",
            "51", "52", "53", "54", "55", "56", "57", "58", "59", "60",
            "61", "62", "63", "64", "65", "66", "67", "68", "69", "70",
            "71", "72", "73", "74", "75", "76", "77", "78", "79", "80",
            "81", "82", "83", "84", "85", "86", "87", "88", "89", "90",
            "91", "92", "93", "94", "95", "96", "97", "98", "99", "100"
    };

    private int floorIdx = 0;
    private String[] floorItem = null;
    private String floorName = "";

    public PoseStampViewModel() {
        List<PoseStamp> initPoseStampList = new ArrayList<>();
        poseStampList.setValue(initPoseStampList);

        List<String> initLabelList = new ArrayList<>();
        poseStampLabelList.setValue(initLabelList);
    }

    public LiveData<List<PoseStamp>> getPoseStampList() {
        return poseStampList;
    }
    public int getPoseStampListSize() {
        return Objects.requireNonNull(poseStampList.getValue()).size();
    }
    public void addPoseStamp(PoseStamp newItem) {
        List<PoseStamp> currentPoseStampList = poseStampList.getValue();
        List<String> currentPoseStampLabelList = poseStampLabelList.getValue();

        if (currentPoseStampList != null && currentPoseStampLabelList != null) {
            currentPoseStampList.add(newItem);
            currentPoseStampLabelList.add(
                    String.format(Locale.ROOT, "노드 : %d", currentPoseStampList.size())
            );

            poseStampList.setValue(currentPoseStampList);
            poseStampLabelList.setValue(currentPoseStampLabelList);
        }
    }
    public void clearPoseStampList() {
        List<PoseStamp> currentPoseStampList = poseStampList.getValue();
        List<String> currentPoseStampLabelList = poseStampLabelList.getValue();

        if (currentPoseStampList != null && currentPoseStampLabelList != null) {
            currentPoseStampList.clear();
            currentPoseStampLabelList.clear();

            poseStampList.setValue(currentPoseStampList);
            poseStampLabelList.setValue(currentPoseStampLabelList);
        }
    }
    public PoseStamp getPoseStampAt(int index) {
        List<PoseStamp> list = poseStampList.getValue();
        if (list != null && index >= 0 && index < list.size()) {
            return list.get(index);
        }
        return null;
    }
    public LiveData<List<String>> getPoseStampLabelList() {
        return poseStampLabelList;
    }

    public void updateLabel(int index, String newLabel) {
        List<String> list = poseStampLabelList.getValue();
        if (list != null && index < list.size()) {
            list.set(index, newLabel);
            poseStampLabelList.setValue(list);
        }
    }

    public String getLabelAt(int index) {
        List<String> list = poseStampLabelList.getValue();
        if (list != null && index >= 0 && index < list.size()) {
            return list.get(index);
        }
        return null;
    }

    public void updateBuildingName(String buildingName){ this.buildingName = buildingName; }
    public String getBuildingName(){ return this.buildingName; }

    public int getFloorMinIdx(){ return this.floorMinIdx; }
    public String[] getFloorMinItem() { return this.floorMinItem; }
    public void updateFloorMinIdx(int index) { this.floorMinIdx = index; }

    public int getFloorMaxIdx(){ return this.floorMaxIdx; }
    public String[] getFloorMaxItem() { return this.floorMaxItem; }
    public void updateFloorMaxIdx(int index) { this.floorMaxIdx = index; }

    public int getFloorIdx(){ return this.floorIdx; }
    public void updateFloorItem() {
        int minIdx = this.getFloorMinIdx();
        int maxIdx = this.getFloorMaxIdx();

        this.floorItem = new String[minIdx + maxIdx + 1];

        int idx = 0;
        for(int i = maxIdx; i >= 0; i--){
            floorItem[idx] = this.getFloorMaxItem()[i];
            idx += 1;
        }

        for(int i = 0; i < minIdx; i++){
            floorItem[idx] = this.getFloorMinItem()[i + 1];
            idx += 1;
        }
    }
    public String[] getFloorItem() { return this.floorItem; }
    public void updateFloorIdx(int index) { this.floorIdx = index; }

    public String getFloorName(){ return this.floorName; }
    public void updateFloorName(String floorName){ this.floorName = floorName; }
}