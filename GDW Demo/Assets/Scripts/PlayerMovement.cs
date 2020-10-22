using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PlayerMovement : MonoBehaviour
{
    public float speed = 10.0f;
	public Camera cam;
    public float factor = 1.0f;
    public GameObject bullet;
    public float bulletspeed;
    public float cooldown;
    private float timer;


    bool isGrounded = false;
    // Start is called before the first frame update
    void Start()
    { 
        Physics.IgnoreCollision(bullet.GetComponent<Collider>(), GetComponent<Collider>());
        timer = cooldown;
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

        timer -= Time.deltaTime;

        if (Input.GetKey(KeyCode.Space) && timer <= 0)
        {
            GameObject bulletClone;
            bulletClone = Instantiate(bullet, transform.position, transform.rotation);
            timer = cooldown;
        }

        var dist = Vector3.Distance(transform.position, cam.transform.position) * factor;
        Vector3 pos = Input.mousePosition;
        pos.z = dist;
        pos = Camera.main.ScreenToWorldPoint(pos);
        transform.LookAt(pos, Vector3.up);


        //Makes player move
        Vector3 InputDir = PosControl.normalized;
        Vector3 NewPos = transform.position + InputDir.normalized * speed * Time.deltaTime;
        transform.position = NewPos;
    }
}


