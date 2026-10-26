package com.example.whitecaneguardian

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.RadioGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.firestore.FirebaseFirestore

class RfidFragment : Fragment(), BleManager.Listener {

    private lateinit var etName: EditText
    private lateinit var etObstacle: EditText
    private lateinit var tvHeight: TextView
    private lateinit var rgVib: RadioGroup
    private lateinit var btnSend: Button
    private lateinit var btnRegister: Button
    private lateinit var tvStatus: TextView

    private val uiHandler = Handler(Looper.getMainLooper())
    private var requestToken = 0
    private var cfgAcknowledged = false
    private var registering = false
    private var awaitingConfig = false
    private var viewAlive = false
    private var heightCode: Int? = null
    private lateinit var bleManager: BleManager

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View = inflater.inflate(R.layout.fragment_rfid, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        etName = view.findViewById(R.id.etRfidName)
        etObstacle = view.findViewById(R.id.etRfidObstacle)
        tvHeight = view.findViewById(R.id.tvRfidHeight)
        rgVib = view.findViewById(R.id.rgVibration)
        btnSend = view.findViewById(R.id.btnRfidSend)
        btnRegister = view.findViewById(R.id.btnRfidRegister)
        tvStatus = view.findViewById(R.id.tvRfidStatus)

        viewAlive = true
        btnRegister.isEnabled = false
        loadHeightFromProfile()

        btnSend.setOnClickListener { onSendConfig() }
        btnRegister.setOnClickListener { onRegister() }

        bleManager = BleHolder.get(requireContext())
        bleManager.addListener(this)
        onStatusChanged(bleManager.isReady())
    }

    private fun loadHeightFromProfile() {
        val uid = FirebaseAuth.getInstance().currentUser?.uid
        if (uid == null) {
            tvHeight.text = "로그인이 필요합니다"
            return
        }
        FirebaseFirestore.getInstance().collection("users").document(uid)
            .get()
            .addOnSuccessListener { doc ->
                if (!isAdded || !viewAlive) return@addOnSuccessListener
                val height = doc.getDouble("height")
                if (height == null) {
                    tvHeight.text = "신체 정보에 키가 없습니다 (먼저 등록해주세요)"
                    heightCode = null
                } else {
                    val code = (height / 10).toInt()
                    heightCode = code
                    tvHeight.text = "${height.toInt()}cm (코드 $code)"
                }
            }
            .addOnFailureListener {
                if (isAdded && viewAlive) tvHeight.text = "키를 불러오지 못했습니다"
            }
    }

    private fun onSendConfig() {
        val name = etName.text.toString().trim()
        val obstacle = etObstacle.text.toString().toIntOrNull()
        val code = heightCode

        if (!RfidConfig.NAME_REGEX.matches(name)) {
            tvStatus.text = "이름은 영문/숫자 1~12자여야 합니다 (공백·쉼표 불가)"
            return
        }
        if (obstacle == null || obstacle < RfidConfig.OBSTACLE_MIN_CM || obstacle > RfidConfig.OBSTACLE_MAX_CM) {
            tvStatus.text = "장애물 거리는 ${RfidConfig.OBSTACLE_MIN_CM}~${RfidConfig.OBSTACLE_MAX_CM}cm 사이여야 합니다"
            return
        }
        if (code == null || code < RfidConfig.HEIGHT_CODE_MIN || code > RfidConfig.HEIGHT_CODE_MAX) {
            tvStatus.text = "키는 ${RfidConfig.HEIGHT_CODE_MIN * 10}~${RfidConfig.HEIGHT_CODE_MAX * 10 + 9}cm 범위여야 합니다. 신체 정보를 확인해주세요."
            return
        }
        val vib = if (rgVib.checkedRadioButtonId == R.id.rbVibStrong)
            RfidConfig.VIB_STRONG else RfidConfig.VIB_WEAK

        val cfg = "CFG,$name,$obstacle,$code,$vib\n"

        if (!bleManager.isReady()) {
            tvStatus.text = "지팡이가 연결되어 있지 않습니다"
            return
        }
        val ok = bleManager.sendCommand(cfg)
        if (ok) {
            tvStatus.text = "설정 전송 중... (응답 대기)"
            cfgAcknowledged = false
            awaitingConfig = true
            btnSend.isEnabled = false
            btnRegister.isEnabled = false
            armTimeout(RfidConfig.ACK_TIMEOUT_MS, "설정 응답 시간 초과. 다시 전송해주세요.")
        } else {
            tvStatus.text = "전송 실패 (연결 상태 확인)"
        }
    }

    private fun onRegister() {
        if (!cfgAcknowledged || registering) { tvStatus.text = "먼저 설정 전송을 완료해주세요."; return }
        if (!bleManager.isReady()) {
            tvStatus.text = "지팡이가 연결되어 있지 않습니다"
            return
        }
        val ok = bleManager.sendCommand("REG\n")
        tvStatus.text = if (ok) "카드 등록 요청 중..." else "전송 실패"
        if (ok) { registering = true; btnSend.isEnabled = false; btnRegister.isEnabled = false
            armTimeout(RfidConfig.ACK_TIMEOUT_MS, "등록 응답 시간 초과. 다시 시도해주세요.") }
    }

    override fun onStatusChanged(connected: Boolean) {
        if (!viewAlive) return
        if (!connected) {
            ++requestToken; cfgAcknowledged = false; registering = false; awaitingConfig = false
            tvStatus.text = "지팡이 연결이 끊겼습니다. 연결 후 설정을 다시 전송해주세요."
        }
        btnSend.isEnabled = connected
        btnRegister.isEnabled = connected && cfgAcknowledged && !registering
    }
    private fun armTimeout(delayMs: Long, message: String) {
        val token = ++requestToken
        uiHandler.postDelayed({
            if (viewAlive && token == requestToken) {
                registering = false; awaitingConfig = false
                tvStatus.text = message
                btnSend.isEnabled = bleManager.isReady()
                btnRegister.isEnabled = bleManager.isReady() && cfgAcknowledged
            }
        }, delayMs)
    }
    private fun endRegistration(message: String) {
        ++requestToken; registering = false
        tvStatus.text = message
        btnSend.isEnabled = bleManager.isReady()
        btnRegister.isEnabled = bleManager.isReady() && cfgAcknowledged
    }
    override fun onMessageReceived(message: String) {}

    override fun onRfidResponse(line: String) {
        activity?.runOnUiThread { if (viewAlive) handleRfidResponse(line) }
    }

    private fun handleRfidResponse(line: String) {
        val parts = line.split(",")
        when (parts.getOrNull(0)) {
            "ACK" -> if (parts.getOrNull(1) == "CFG" && awaitingConfig) {
                ++requestToken; awaitingConfig = false; cfgAcknowledged = true
                tvStatus.text = "대기 설정 전송 완료. 카드 등록을 진행하세요."
                btnSend.isEnabled = bleManager.isReady()
                btnRegister.isEnabled = bleManager.isReady()
            }
            "ERR" -> if (parts.getOrNull(1) == "CFG" || parts.getOrNull(1) == "QUEUE" || parts.getOrNull(1) == "COMMAND") {
                ++requestToken; awaitingConfig = false; cfgAcknowledged = false
                endRegistration("설정 오류: ${parts.getOrNull(2) ?: "UNKNOWN"}")
            }
            "REG" -> when (parts.getOrNull(1)) {
                "WAIT" -> {
                    registering = true; btnSend.isEnabled = false; btnRegister.isEnabled = false
                    val seconds = parts.getOrNull(2)?.toLongOrNull()?.coerceIn(1L, 60L) ?: 15L
                    tvStatus.text = "카드를 지팡이에 대주세요 (${seconds}초)"
                    armTimeout(seconds * 1000 + RfidConfig.ACK_TIMEOUT_MS, "등록 결과 응답 시간 초과. 카드 상태를 확인해주세요.")
                }
                "OK" -> {
                    val mode = if (parts.getOrNull(3) == "UPDATE") "덮어쓰기" else "신규"
                    endRegistration("등록 완료 ($mode): ${parts.getOrNull(4) ?: ""}")
                }
                "TIMEOUT" -> endRegistration("시간 초과. 다시 시도해주세요.")
                "NOCFG" -> { cfgAcknowledged = false; endRegistration("먼저 설정을 전송해주세요.") }
                "NO_RFID" -> endRegistration("RFID 모듈이 준비되지 않았습니다. 연결 후 다시 시도해주세요.")
                "FULL" -> endRegistration("등록 가능 인원을 초과했습니다 (최대 500명).")
                "FSERR" -> endRegistration("저장소 오류입니다. 시리얼에서 저장소 상태를 확인해주세요.")
                "CANCEL" -> endRegistration("등록 취소: ${parts.getOrNull(2) ?: "USER"}")
            }
            "APPLY" -> tvStatus.text = "등록된 카드 인식: ${parts.getOrNull(2) ?: ""}"
            "UNREG" -> tvStatus.text = "등록되지 않은 카드입니다. 기본 모드로 전환되었습니다."
        }
    }

    override fun onDestroyView() {
        if (registering && bleManager.isReady()) bleManager.sendCommand("REG_CANCEL")
        viewAlive = false; ++requestToken
        uiHandler.removeCallbacksAndMessages(null)
        bleManager.removeListener(this)
        super.onDestroyView()
    }
}