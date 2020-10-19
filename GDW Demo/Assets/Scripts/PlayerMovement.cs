using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PlayerMovement : MonoBehaviour
{
    Rigidbody rigidbody;
    public float speed = 10.0f;


    bool isGrounded = false;
    // Start is called before the first frame update
    void Start()
    {
        rigidbody = GetComponent<Rigidbody>();
    }

    // Update is called once per frame
    void Update()
    {
        Vector3 PosControl = Vector3.zero;

        if (Input.GetKey(KeyCode.W))
        {
            PosControl.z = 1.0f;

        }

        if (Input.GetKey(KeyCode.A))
        {
            PosControl.x = -1.0f;
        }

        if (Input.GetKey(KeyCode.S))
        {
            PosControl.z = -1.0f;
        }

        if (Input.GetKey(KeyCode.D))
        {
            PosControl.x = 1.0f;
        }

        //Makes player move
        Vector3 InputDir = PosControl.normalized;
        Vector3 NewPos = transform.position + InputDir.normalized * speed * Time.deltaTime;
        transform.position = NewPos;

    }
}


