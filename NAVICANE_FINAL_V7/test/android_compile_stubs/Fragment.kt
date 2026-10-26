package androidx.fragment.app
import android.os.Bundle
import android.view.View
import android.view.LayoutInflater
import android.view.ViewGroup
import android.content.Context
import android.app.Activity
open class Fragment {
 val isAdded=true
 val activity:Activity?=null
 fun requireContext():Context=throw UnsupportedOperationException()
 open fun onCreateView(inflater:LayoutInflater,container:ViewGroup?,savedInstanceState:Bundle?):View?=null
 open fun onViewCreated(view:View,savedInstanceState:Bundle?) {}
 open fun onDestroyView() {}
}
