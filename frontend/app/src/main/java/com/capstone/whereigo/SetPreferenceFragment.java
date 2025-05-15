package com.capstone.whereigo;

import android.content.Intent;
import android.os.Bundle;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

public class SetPreferenceFragment extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.download_preference, rootKey);

        Preference mapItemPreference = findPreference("download_map");
        if (mapItemPreference != null) {
            mapItemPreference.setOnPreferenceClickListener(preference -> {
                TextView tv = requireActivity().findViewById(R.id.toolbar_title);
                tv.setText(R.string.download_title);

                requireActivity().getSupportFragmentManager().beginTransaction()
                        .addToBackStack(null)
                        .replace(R.id.setting_list, new DownloadFragment())
                        .commit();
                return true;
            });
        }

        Preference makeMapPreference = findPreference("make_map");
        if (makeMapPreference != null) {
            makeMapPreference.setOnPreferenceClickListener(preference -> {
                Intent intent = new Intent(requireContext(), MappingActivity.class);
                startActivity(intent);
                requireActivity().finish();
                return true;
            });
        }
    }
}
