package com.example.whitecaneguardian

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import java.io.ByteArrayOutputStream
import java.util.UUID
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CopyOnWriteArraySet

/** NAVCANE_Final protocol. Existing public listener/sendCommand API is retained. */
class BleManager(private val context: Context) {
    companion object {
        const val DEVICE_NAME = "NAVCANE_ESP32"
        val SERVICE_UUID: UUID = UUID.fromString("12345678-1234-1234-1234-1234567890ab")
        val CHARACTERISTIC_UUID: UUID = UUID.fromString("abcd1234-1234-1234-1234-abcdef123456")
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val REQUEST_MTU = 185
        private const val MAX_LINE_BYTES = 96
        private const val MAX_QUEUE = 32
    }
    interface Listener {
        fun onStatusChanged(connected: Boolean)
        fun onMessageReceived(message: String)
        fun onRfidResponse(line: String) {}
    }
    // Primary listener remains available to the app's fall/emergency handler.
    @Volatile var listener: Listener? = null
    private val additionalListeners = CopyOnWriteArraySet<Listener>()
    fun addListener(value: Listener) { additionalListeners.add(value) }
    fun removeListener(value: Listener) { additionalListeners.remove(value) }
    private fun listeners(): Set<Listener> = additionalListeners.toSet() + listOfNotNull(listener)

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private var scanner: BluetoothLeScanner? = null
    @Volatile private var gatt: BluetoothGatt? = null
    private var scanning = false
    private var scanGeneration = 0
    @Volatile private var connected = false
    @Volatile private var writeCharacteristic: BluetoothGattCharacteristic? = null
    private val handler = Handler(Looper.getMainLooper())
    @Volatile private var writeReady = false
    private var writeInProgress = false
    private var servicesRequested = false
    private var negotiatedMtu = 23
    private val writeQueue = ConcurrentLinkedQueue<ByteArray>()
    private var currentWrite: ByteArray? = null
    private var writeOffset = 0
    private var lastChunkSize = 0
    private var writeToken = 0
    private var writeRetries = 0
    private var autoReconnect = true
    private val lines = NavcaneLines(MAX_LINE_BYTES)

    private fun log(type: String, detail: String) {
        BleLog.add(BleLogEntry(System.currentTimeMillis(), type, detail))
    }
    @SuppressLint("MissingPermission")
    fun startScan() {
        if (scanning || connected || gatt != null || bluetoothAdapter?.isEnabled != true) return
        autoReconnect = true
        scanner = bluetoothAdapter.bluetoothLeScanner ?: return
        scanning = true
        scanner?.startScan(scanCallback)
        val token = ++scanGeneration
        handler.postDelayed({
            if (scanning && token == scanGeneration) {
                stopScan()
                if (autoReconnect) handler.postDelayed({ if (autoReconnect && gatt == null) startScan() }, 2000)
            }
        }, 10000)
    }
    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (!scanning) return
        scanning = false; ++scanGeneration
        scanner?.stopScan(scanCallback)
    }
    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (result.device.name != DEVICE_NAME) return
            handler.post {
                if (!scanning || gatt != null) return@post
                log("연결", "기기 발견: $DEVICE_NAME")
                stopScan()
                gatt = result.device.connectGatt(context, false, gattCallback)
                val pending = gatt
                handler.postDelayed({
                    if (gatt === pending && !connected && pending != null) failConnection(pending, "연결 시간 초과")
                }, 10000)
            }
        }
        override fun onScanFailed(errorCode: Int) {
            handler.post { scanning = false; log("오류", "BLE 검색 실패: $errorCode") }
        }
    }
    @SuppressLint("MissingPermission")
    private fun discover(g: BluetoothGatt) {
        if (gatt !== g || servicesRequested) return
        servicesRequested = true
        if (!g.discoverServices()) { failConnection(g, "서비스 검색 요청 실패"); return }
        handler.postDelayed({
            if (gatt === g && !writeReady) failConnection(g, "BLE 준비 시간 초과")
        }, 10000)
    }
    @SuppressLint("MissingPermission")
    private val gattCallback: BluetoothGattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            handler.post {
                if (gatt !== g) { g.close(); return@post }
                if (status != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) {
                    failConnection(g, "연결 끊김 (status=$status)")
                } else if (newState == BluetoothProfile.STATE_CONNECTED) {
                    connected = true
                    log("연결", "기기 연결됨")
                    // Ready notification is published only after a successful CCCD write.
                    if (!g.requestMtu(REQUEST_MTU)) discover(g)
                    else handler.postDelayed({ discover(g) }, 3000)
                }
            }
        }
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            handler.post {
                if (gatt !== g) return@post
                negotiatedMtu = if (status == BluetoothGatt.GATT_SUCCESS) mtu.coerceAtLeast(23) else 23
                log("연결", "MTU: $negotiatedMtu (status=$status)")
                discover(g)
            }
        }
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            handler.post {
                if (gatt !== g) return@post
                if (status != BluetoothGatt.GATT_SUCCESS) { failConnection(g, "서비스 검색 실패"); return@post }
                val ch = g.getService(SERVICE_UUID)?.getCharacteristic(CHARACTERISTIC_UUID)
                if (ch == null) { failConnection(g, "NAVCANE 특성 없음"); return@post }
                writeCharacteristic = ch
                val descriptor = ch.getDescriptor(CCCD_UUID)
                if (descriptor == null || !g.setCharacteristicNotification(ch, true)) {
                    failConnection(g, "알림 설정 실패"); return@post
                }
                @Suppress("DEPRECATION")
                val started = if (Build.VERSION.SDK_INT >= 33) {
                    g.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
                } else {
                    descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    g.writeDescriptor(descriptor)
                }
                if (!started) failConnection(g, "알림 요청 실패")
            }
        }
        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            handler.post {
                if (gatt !== g || descriptor.uuid != CCCD_UUID || writeReady) return@post
                if (status != BluetoothGatt.GATT_SUCCESS) { failConnection(g, "알림 준비 실패"); return@post }
                writeReady = true
                log("연결", "알림 준비 완료")
                // Opt into newline framing. Old app remains supported by firmware's default mode.
                currentWrite = "STREAM_ON\n".toByteArray(Charsets.UTF_8)
                writeOffset = 0
                sendCommand("GET_STATUS")
                listeners().forEach { it.onStatusChanged(true) }
                processWriteQueue()
            }
        }
        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            handler.post {
                if (gatt !== g || characteristic.uuid != CHARACTERISTIC_UUID || !writeInProgress) return@post
                writeInProgress = false
                ++writeToken
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    // A failed chunk may have reached ESP32. Do not retry a whole CFG/REG blindly.
                    failConnection(g, "명령 전송 실패 (status=$status)"); return@post
                }
                writeOffset += lastChunkSize
                writeRetries = 0
                if (writeOffset >= (currentWrite?.size ?: 0)) { currentWrite = null; writeOffset = 0 }
                processWriteQueue()
            }
        }
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            // API 33+ uses the value overload; avoid delivering the same notification twice.
            if (Build.VERSION.SDK_INT < 33) receive(g, characteristic.uuid, characteristic.value?.copyOf() ?: byteArrayOf())
        }
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            receive(g, characteristic.uuid, value.copyOf())
        }
    }
    private fun receive(g: BluetoothGatt, uuid: UUID, bytes: ByteArray) {
        handler.post {
            if (gatt !== g || uuid != CHARACTERISTIC_UUID) return@post
            val previousDrops = lines.droppedLines
            lines.push(bytes).forEach { dispatch(it) }
            if (lines.droppedLines != previousDrops) log("오류", "BLE 메시지 길이/형식 오류")
        }
    }
    private fun dispatch(message: String) {
        logParsedEvent(message)
        listeners().forEach { value ->
            value.onMessageReceived(message)
            if (isRfidResponse(message)) value.onRfidResponse(message)
        }
    }
    private fun logParsedEvent(message: String) {
        val (type, detail) = when {
            message == "FALL_DETECTED" -> "낙상" to "낙상 사고 감지"
            message.startsWith("OBSTACLE:") -> "장애물" to when (message.substringAfter(":")) {
                "NONE" -> "좌우 장애물 사라짐"
                "LEFT" -> "좌측 장애물"
                "RIGHT" -> "우측 장애물"
                "BOTH" -> "양측 장애물 (좌우)"
                "UPPER", "FRONT" -> "상부 장애물"
                "UPPER_CLEAR", "FRONT_CLEAR" -> "상부 장애물 사라짐"
                else -> message
            }
            message.startsWith("STAIR_DETECTED") -> "계단" to "계단(내리막) 감지"
            message.startsWith("계단 감지") -> "계단" to "계단(내리막) 감지"
            message == "STAIR_CLEARED" -> "계단" to "계단 해제"
            message.startsWith("포고핀 분리") -> "기기" to "포고핀 분리됨"
            message.startsWith("포고핀 재결합") -> "기기" to "포고핀 재결합됨"
            message == "BUTTON_PRESSED" -> { VoiceTrigger.fire(); "버튼" to "버튼 눌림" }
            message == "GREEN_LIGHT" -> "신호등" to "초록불 감지"
            message == "CONNECTED" -> "연결" to "기기 준비됨 (CONNECTED)"
            message.startsWith("LED:") -> "상태" to "LED ${message.substringAfter(":")}"
            message == "PONG" -> "상태" to "PONG"
            message.startsWith("SENSOR:") -> "센서" to message
            message.startsWith("ACCEL:") -> "센서" to "가속도 ${message.substringAfter(":")}"
            message.startsWith("DISTANCE:") -> "센서" to "전방거리 ${message.substringAfter(":")}cm"
            message.startsWith("STAIR_DISTANCE:") -> "센서" to "계단거리 ${message.substringAfter(":")}cm"
            isRfidResponse(message) -> "RFID" to message
            message.startsWith("ERROR:") -> "오류" to message
            else -> "메시지" to message
        }
        log(type, detail)
    }
    private fun isRfidResponse(msg: String): Boolean =
        msg.startsWith("ACK,") || msg.startsWith("ERR,") || msg.startsWith("REG,") ||
        msg.startsWith("APPLY,") || msg.startsWith("UNREG,")

    fun isConnected(): Boolean = connected
    fun isReady(): Boolean = connected && writeReady
    // Return value means accepted into the local queue, not acknowledged by ESP32.
    fun sendCommand(command: String): Boolean {
        if (!connected || gatt == null || writeCharacteristic == null || writeQueue.size >= MAX_QUEUE) return false
        val framed = if (command.endsWith("\n")) command else "$command\n"
        if (framed.toByteArray(Charsets.UTF_8).size > MAX_LINE_BYTES + 1 || framed.indexOf('\u0000') >= 0) return false
        writeQueue.add(framed.toByteArray(Charsets.UTF_8))
        handler.post { processWriteQueue() }
        return true
    }
    @SuppressLint("MissingPermission")
    private fun processWriteQueue() {
        if (!writeReady || writeInProgress) return
        val g = gatt ?: return
        val ch = writeCharacteristic ?: return
        if (currentWrite == null) { currentWrite = writeQueue.poll() ?: return; writeOffset = 0 }
        val data = currentWrite ?: return
        val end = (writeOffset + (negotiatedMtu - 3).coerceAtLeast(20)).coerceAtMost(data.size)
        val chunk = data.copyOfRange(writeOffset, end)
        lastChunkSize = chunk.size
        writeInProgress = true
        @Suppress("DEPRECATION")
        val started = if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(ch, chunk, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
        } else {
            ch.value = chunk; ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            g.writeCharacteristic(ch)
        }
        if (!started) {
            writeInProgress = false
            if (++writeRetries > 3) { failConnection(g, "명령 전송 요청 실패"); return }
            handler.postDelayed({ if (gatt === g) processWriteQueue() }, 150)
            return
        }
        val token = ++writeToken
        handler.postDelayed({
            if (gatt === g && writeInProgress && writeToken == token) failConnection(g, "명령 전송 시간 초과")
        }, 5000)
    }
    @SuppressLint("MissingPermission")
    private fun failConnection(g: BluetoothGatt, reason: String) {
        if (gatt !== g) return
        gatt = null; connected = false; writeReady = false; writeInProgress = false
        servicesRequested = false; negotiatedMtu = 23; writeCharacteristic = null
        writeQueue.clear(); currentWrite = null; writeOffset = 0; writeRetries = 0; ++writeToken
        lines.clear()
        g.disconnect(); g.close()
        log("해제", reason)
        listeners().forEach { it.onStatusChanged(false) }
        if (autoReconnect) handler.postDelayed({ if (autoReconnect && gatt == null) startScan() }, 2000)
    }
    @SuppressLint("MissingPermission")
    fun disconnect() {
        autoReconnect = false
        stopScan()
        gatt?.let { failConnection(it, "사용자가 연결 해제") }
    }
}

/** Byte framing is independent of Android so UTF-8 split boundaries are testable on JVM. */
internal class NavcaneLines(private val maxBytes: Int = 96) {
    private val bytes = ByteArrayOutputStream()
    private var discarding = false
    var droppedLines: Int = 0
        private set
    fun clear() { bytes.reset(); discarding = false; droppedLines = 0 }
    fun push(chunk: ByteArray): List<String> {
        val result = mutableListOf<String>()
        for (byte in chunk) {
            when (byte.toInt() and 0xff) {
                13 -> Unit
                10 -> {
                    if (!discarding) {
                        val line = bytes.toByteArray().toString(Charsets.UTF_8).trim()
                        if (line.isNotEmpty()) result.add(line)
                    }
                    bytes.reset(); discarding = false
                }
                else -> if (!discarding) {
                    if (bytes.size() >= maxBytes || byte.toInt() == 0) {
                        bytes.reset(); discarding = true; ++droppedLines
                    } else bytes.write(byte.toInt())
                }
            }
        }
        return result
    }
}
