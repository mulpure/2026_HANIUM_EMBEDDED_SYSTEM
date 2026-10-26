package com.google.firebase.auth
class FirebaseUser { val uid:String?="user" }
class FirebaseAuth {
 val currentUser:FirebaseUser?=FirebaseUser()
 companion object { fun getInstance():FirebaseAuth=FirebaseAuth() }
}
