package com.example.whitecaneguardian
import android.content.Context
object BleLog { fun add(entry: BleLogEntry) {} }
data class BleLogEntry(val time:Long,val type:String,val detail:String)
object VoiceTrigger { var calls=0; fun fire() { ++calls } }
object BleHolder { fun get(context:Context):BleManager=BleManager(context) }
object R {
 object layout { const val fragment_rfid=1 }
 object id { const val etRfidName=1; const val etRfidObstacle=2; const val tvRfidHeight=3; const val rgVibration=4; const val btnRfidSend=5; const val btnRfidRegister=6; const val tvRfidStatus=7; const val rbVibStrong=8 }
}
