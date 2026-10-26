package com.google.firebase.firestore
class DocumentSnapshot { fun getDouble(key:String):Double?=170.0 }
class Task {
 fun addOnSuccessListener(listener:(DocumentSnapshot)->Unit):Task=this
 fun addOnFailureListener(listener:(Exception)->Unit):Task=this
}
class CollectionReference { fun document(path:String):DocumentReference=DocumentReference() }
class DocumentReference { fun get():Task=Task() }
class FirebaseFirestore {
 fun collection(path:String):CollectionReference=CollectionReference()
 companion object { fun getInstance():FirebaseFirestore=FirebaseFirestore() }
}
