package com.simpleenergy.buttonapp

import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "SimpleButton"
    }

    private lateinit var statusText: TextView
    private var simulatedValue = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.statusText)

        findViewById<Button>(R.id.btnRead).setOnClickListener {
            readButtonValue()
        }

        findViewById<Button>(R.id.btnTrigger).setOnClickListener {
            triggerButtonClick()
        }
    }

    private fun readButtonValue() {
        simulatedValue = if (simulatedValue == 0) 1 else 0
        statusText.text = "Button value: $simulatedValue"
    }

    private fun triggerButtonClick() {
        Log.d(TAG, "simplebutton: clicked")
        statusText.text = "Triggered! Check logcat"
    }
}
